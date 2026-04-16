#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#include "../include/sbrk_allocator.h"
#include "../include/sbrk_list_allocator.h"
#include "../include/opt_allocator.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

#define MEM_ITERS   1000  /**< Total alloc/free operations per scenario.   */
#define CYCLIC_WIN    10  /**< Live-block window size for cyclic scenarios. */

/**
 * @brief Sizes cycled through in mixed-size scenarios.
 *
 * Chosen to span a wide range: tiny, small, medium, large.
 */
static const size_t MIXED_SIZES[] = { 16, 128, 1024, 8192 };
#define MIXED_N  (int)(sizeof MIXED_SIZES / sizeof MIXED_SIZES[0])

/* ------------------------------------------------------------------ */
/* Types                                                                */
/* ------------------------------------------------------------------ */

/** @brief Signature of an allocator function (e.g. sbrk_malloc). */
typedef void *(*alloc_fn_t)(size_t);

/** @brief Signature of a deallocator function (e.g. sbrk_free). */
typedef void  (*free_fn_t)(void *);

/**
 * @brief Free-order strategy for a memory-usage scenario.
 *
 * - INTERLEAVED: each block is freed immediately after allocation;
 *   at most one block is live at any time.
 * - FIFO / LIFO: all MEM_ITERS blocks are allocated first, then freed
 *   in allocation order (FIFO) or reverse order (LIFO).
 * - CYCLIC_FIFO / CYCLIC_LIFO: blocks are allocated and freed in a
 *   sliding window of CYCLIC_WIN blocks, repeating MEM_ITERS/CYCLIC_WIN
 *   times. Within each window the free order is FIFO or LIFO.
 *   Example (window=3): alloc a,b,c → free a,b,c → alloc d,e,f → ...
 */
typedef enum {
    ORDER_INTERLEAVED = 0,
    ORDER_FIFO        = 1,
    ORDER_LIFO        = 2,
    ORDER_CYCLIC_FIFO = 3,
    ORDER_CYCLIC_LIFO = 4,
} free_order_t;

/**
 * @brief Heap-usage statistics measured in a child process.
 *
 * Both fields are program-break deltas relative to a snapshot taken
 * immediately before the first alloc_fn call.
 */
typedef struct {
    size_t peak_bytes;     /**< Max heap extension observed while blocks are live. */
    size_t residual_bytes; /**< Heap not reclaimed after all blocks are freed.     */
} mem_stats_t;

/* ------------------------------------------------------------------ */
/* Scenario (runs inside a forked child)                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Execute one memory-usage scenario and return heap statistics.
 *
 * The baseline is taken immediately before the first alloc_fn call so
 * that any libc overhead (e.g. the pointer-array allocation) is excluded.
 * This function is always called inside a forked child process, so the
 * allocator's static state and the program break are pristine on entry.
 *
 * @param iters     Total alloc/free operations.
 * @param size      Bytes per block, or 0 to cycle through MIXED_SIZES.
 * @param order     Free-order strategy.
 * @param alloc_fn  Allocator to exercise.
 * @param free_fn   Deallocator matching @p alloc_fn.
 * @return mem_stats_t with peak_bytes and residual_bytes, or {0,0} on OOM.
 */
static mem_stats_t scenario_mem(int iters, size_t size, free_order_t order,
                                alloc_fn_t alloc_fn, free_fn_t free_fn)
{
    mem_stats_t s = { 0, 0 };

    switch (order) {

    case ORDER_INTERLEAVED: {
        /* Baseline taken here — no libc allocation precedes the loop. */
        char *baseline = (char *)sbrk(0);
        size_t peak_bytes = 0;
        for (int i = 0; i < iters; i++) {
            void *p = alloc_fn(size ? size : MIXED_SIZES[i % MIXED_N]);
            peak_bytes = max(peak_bytes, (size_t)((char *)sbrk(0) - baseline));
            free_fn(p);
        }
        s.peak_bytes     = peak_bytes;
        s.residual_bytes = (size_t)((char *)sbrk(0) - baseline);
        break;
    }

    case ORDER_FIFO:
    case ORDER_LIFO: {
        /* Allocate the pointer array first (may touch libc/sbrk), then
         * snapshot the break so the array does not skew the delta. */
        void **ptrs = (void **)malloc((size_t)iters * sizeof(void *));
        if (!ptrs) break;
        char *baseline = (char *)sbrk(0);

        for (int i = 0; i < iters; i++)
            ptrs[i] = alloc_fn(size ? size : MIXED_SIZES[i % MIXED_N]);
        s.peak_bytes = (size_t)((char *)sbrk(0) - baseline);

        if (order == ORDER_FIFO)
            for (int i = 0; i < iters; i++)        free_fn(ptrs[i]);
        else
            for (int i = iters - 1; i >= 0; i--)  free_fn(ptrs[i]);

        s.residual_bytes = (size_t)((char *)sbrk(0) - baseline);
        free(ptrs);
        break;
    }

    case ORDER_CYCLIC_FIFO:
    case ORDER_CYCLIC_LIFO: {
        /* Sliding window: alloc CYCLIC_WIN blocks, free them, repeat.
         * Peak is the maximum break observed at the end of any alloc phase. */
        int    W   = CYCLIC_WIN;
        int    K   = iters / W;
        void **win = (void **)malloc((size_t)W * sizeof(void *));
        if (!win) break;
        char *baseline = (char *)sbrk(0);
        char *max_brk  = baseline;
        size_t peak_bytes = 0;

        for (int k = 0; k < K; k++) {
            for (int i = 0; i < W; i++)
                win[i] = alloc_fn(size ? size : MIXED_SIZES[(k * W + i) % MIXED_N]);
            peak_bytes = max(peak_bytes, (size_t)((char *)sbrk(0) - baseline));
            char *cur = (char *)sbrk(0);
            if (cur > max_brk) max_brk = cur;
            if (order == ORDER_CYCLIC_FIFO)
                for (int i = 0; i < W; i++)        free_fn(win[i]);
            else
                for (int i = W - 1; i >= 0; i--)  free_fn(win[i]);
        }
        s.peak_bytes     = peak_bytes;
        s.residual_bytes = (size_t)((char *)sbrk(0) - baseline);
        free(win);
        break;
    }
    }

    return s;
}

/* ------------------------------------------------------------------ */
/* Isolation via fork                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Run scenario_mem() in an isolated child process.
 *
 * fork() gives the child a private copy of the process address space:
 * every allocator's static head pointer is NULL and the program break is
 * at the OS default, independent of any other scenario.  The result is
 * written to a pipe and read back by the parent.
 *
 * @param iters     Total alloc/free operations.
 * @param size      Bytes per block, or 0 for mixed sizes.
 * @param order     Free-order strategy.
 * @param alloc_fn  Allocator to exercise.
 * @param free_fn   Deallocator matching @p alloc_fn.
 * @return mem_stats_t as produced by scenario_mem() in the child.
 */
static mem_stats_t run_isolated(int iters, size_t size, free_order_t order,
                                alloc_fn_t alloc_fn, free_fn_t free_fn)
{
    int fds[2];
    if (pipe(fds) == -1)
        return (mem_stats_t){ 0, 0 };

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: run scenario, send result, exit. */
        close(fds[0]);
        mem_stats_t s = scenario_mem(iters, size, order, alloc_fn, free_fn);
        (void)!write(fds[1], &s, sizeof s);
        close(fds[1]);
        _exit(0);
    }

    /* Parent: receive result, reap child. */
    close(fds[1]);
    mem_stats_t s = { 0, 0 };
    (void)!read(fds[0], &s, sizeof s);
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return s;
}

/* ------------------------------------------------------------------ */
/* Reporting                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Print one memory-usage result line.
 *
 * @param label          Human-readable scenario name.
 * @param peak_bytes     Peak heap extension during the scenario.
 * @param residual_bytes Heap not reclaimed after all frees.
 * @return void
 */
static void report_mem(const char *label, size_t peak_bytes, size_t residual_bytes)
{
    printf("  %-36s  peak %7.1f KB  residual %7.1f KB\n",
           label,
           peak_bytes     / 1024.0,
           residual_bytes / 1024.0);
}

/**
 * @brief Run all five free-order variants for one (allocator, size) pair.
 *
 * Each variant is run in an isolated child process so that allocator state
 * and the program break are pristine at the start of every measurement.
 *
 * @param label_prefix  Prefix string prepended to each result label.
 * @param iters         Total alloc/free operations.
 * @param size          Block size in bytes (0 = mixed).
 * @param alloc_fn      Allocator to exercise.
 * @param free_fn       Matching deallocator.
 * @return void
 */
#define RUN_ALL_ORDERS(label_prefix, iters, size, alloc_fn, free_fn) do {           \
    mem_stats_t _s;                                                                  \
    _s = run_isolated((iters), (size), ORDER_INTERLEAVED,  (alloc_fn), (free_fn));  \
    report_mem(label_prefix " interleaved", _s.peak_bytes, _s.residual_bytes);      \
    _s = run_isolated((iters), (size), ORDER_FIFO,         (alloc_fn), (free_fn));  \
    report_mem(label_prefix " FIFO",        _s.peak_bytes, _s.residual_bytes);      \
    _s = run_isolated((iters), (size), ORDER_LIFO,         (alloc_fn), (free_fn));  \
    report_mem(label_prefix " LIFO",        _s.peak_bytes, _s.residual_bytes);      \
    _s = run_isolated((iters), (size), ORDER_CYCLIC_FIFO,  (alloc_fn), (free_fn));  \
    report_mem(label_prefix " cyclic FIFO", _s.peak_bytes, _s.residual_bytes);      \
    _s = run_isolated((iters), (size), ORDER_CYCLIC_LIFO,  (alloc_fn), (free_fn));  \
    report_mem(label_prefix " cyclic LIFO", _s.peak_bytes, _s.residual_bytes);      \
} while (0)

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Run all memory-usage scenarios and print a summary table.
 *
 * Each scenario executes in a dedicated child process (fork + pipe) so
 * that the allocator's internal state and the program break are clean
 * for every measurement.  Two block-size regimes are tested (fixed 256 B
 * and mixed 16/128/1K/8K) against five free-order strategies for each
 * of the three sbrk-based allocators, single-threaded only.
 *
 * @return 0 on success.
 */
int main(void)
{
    printf("=== Heap Allocator Memory Usage Profile ===\n");
    printf("%d ops total, cyclic window = %d blocks\n", MEM_ITERS, CYCLIC_WIN);
    printf("Each scenario runs in an isolated child process.\n\n");
    printf("peak     = max heap extension while blocks are live\n"
           "residual = heap extension not reclaimed after all frees\n\n");

    /* --- Fixed-size blocks --- */
    printf("Fixed-size blocks (256 B):\n\n");

    RUN_ALL_ORDERS("sbrk",      MEM_ITERS, 256, sbrk_malloc,      sbrk_free);      printf("\n");
    RUN_ALL_ORDERS("sbrk_list", MEM_ITERS, 256, sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("opt",       MEM_ITERS, 256, opt_malloc,       opt_free);

    /* --- Mixed-size blocks --- */
    printf("\nMixed-size blocks (16 / 128 / 1K / 8K):\n\n");

    RUN_ALL_ORDERS("sbrk",      MEM_ITERS, 0,   sbrk_malloc,      sbrk_free);      printf("\n");
    RUN_ALL_ORDERS("sbrk_list", MEM_ITERS, 0,   sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("opt",       MEM_ITERS, 0,   opt_malloc,       opt_free);

    printf("\nDone.\n");
    return 0;
}
