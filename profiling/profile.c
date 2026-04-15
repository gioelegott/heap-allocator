#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "timer.h"
#include "../include/allocator.h"
#include "../include/sbrk_allocator.h"
#include "../include/sbrk_list_allocator.h"
#include "../include/opt_allocator.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

#define ITERS        10000   /**< Iterations for single-threaded scenarios. */
#define MT_THREADS   8       /**< Thread count for multi-threaded scenarios. */
#define MT_ITERS     1000    /**< Iterations per thread for MT scenarios.    */

/** @brief Sizes cycled through by mixed-size scenarios. */
static const size_t MIXED_SIZES[] = { 16, 128, 1024, 8192 };
#define MIXED_N  (int)(sizeof MIXED_SIZES / sizeof MIXED_SIZES[0])

/* ------------------------------------------------------------------ */
/* Free-order selector                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Controls the order in which batch-allocated blocks are freed.
 */
typedef enum {
    ORDER_INTERLEAVED = 0, /**< Free each block immediately after allocation. */
    ORDER_FIFO        = 1, /**< Free in allocation order (first-in, first-out). */
    ORDER_LIFO        = 2, /**< Free in reverse order (last-in, first-out). */
} free_order_t;

/* ------------------------------------------------------------------ */
/* Allocator function-pointer types                                     */
/* ------------------------------------------------------------------ */

/** @brief Signature of an allocator function (e.g. malloc, sbrk_malloc). */
typedef void *(*alloc_fn_t)(size_t);

/** @brief Signature of a deallocator function (e.g. free, sbrk_free). */
typedef void  (*free_fn_t)(void *);

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Print a formatted result line.
 *
 * @param label  Human-readable scenario name.
 * @param iters  Total number of operations measured.
 * @param ms     Elapsed time in milliseconds.
 * @return void
 */
static void report(const char *label, int iters, double ms)
{
    printf("  %-58s  %8.2f ms  (%d ops, %.0f ns/op)\n",
           label, ms, iters, ms * 1e6 / iters);
}

/* ------------------------------------------------------------------ */
/* Single-threaded scenarios                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Run a single-threaded allocation scenario with a given allocator.
 *
 * When @p size is 0 the scenario cycles through MIXED_SIZES on each
 * iteration. The @p order parameter selects whether allocations and frees
 * are interleaved or batched (FIFO or LIFO). The pointer array used for
 * batch modes is allocated and freed with this project's own malloc/free
 * (the mmap-backed allocator), not alloc_fn/free_fn, so the pointer
 * bookkeeping does not perturb the allocator under test.
 *
 * @param iters     Number of alloc/free pairs.
 * @param size      Bytes per allocation, or 0 for mixed sizes.
 * @param order     Free-order strategy.
 * @param alloc_fn  Allocator function to benchmark.
 * @param free_fn   Deallocator matching @p alloc_fn.
 * @return Elapsed time in milliseconds, or -1 on allocation failure.
 */
static double scenario_st(int iters, size_t size, free_order_t order,
                           alloc_fn_t alloc_fn, free_fn_t free_fn)
{
    void **ptrs = NULL;

    if (order != ORDER_INTERLEAVED) {
        ptrs = (void **)malloc((size_t)iters * sizeof(void *));
        if (!ptrs) return -1.0;
    }

    prof_timer_t t;
    timer_start(&t);

    if (order == ORDER_INTERLEAVED) {
        for (int i = 0; i < iters; i++) {
            void *p = alloc_fn(size ? size : MIXED_SIZES[i % MIXED_N]);
            free_fn(p);
        }
    } else {
        for (int i = 0; i < iters; i++)
            ptrs[i] = alloc_fn(size ? size : MIXED_SIZES[i % MIXED_N]);
        if (order == ORDER_FIFO)
            for (int i = 0; i < iters; i++)
                free_fn(ptrs[i]);
        else /* ORDER_LIFO */
            for (int i = iters - 1; i >= 0; i--)
                free_fn(ptrs[i]);
    }

    double ms = timer_elapsed_ms(&t);
    free(ptrs);
    return ms;
}

/* ------------------------------------------------------------------ */
/* Multi-threaded scenarios                                             */
/* ------------------------------------------------------------------ */

/** @brief Argument bundle passed to each worker thread. */
typedef struct {
    int          iters;    /**< Number of alloc/free cycles to perform.   */
    size_t       size;     /**< Allocation size in bytes, or 0 for mixed. */
    free_order_t order;    /**< Free-order strategy.                      */
    alloc_fn_t   alloc_fn; /**< Allocator function to benchmark.          */
    free_fn_t    free_fn;  /**< Deallocator matching alloc_fn.            */
} worker_args_t;

/**
 * @brief Thread worker: mirrors scenario_st() using the allocator in @p arg.
 *
 * The pointer bookkeeping array is always allocated with the mmap-backed
 * malloc so that it does not perturb the allocator under test.
 *
 * @param arg Pointer to a worker_args_t.
 * @return NULL always.
 */
static void *mt_worker(void *arg)
{
    const worker_args_t *a = arg;
    void **ptrs = NULL;

    if (a->order != ORDER_INTERLEAVED) {
        ptrs = malloc((size_t)a->iters * sizeof(void *));
        if (!ptrs) return NULL;
    }

    if (a->order == ORDER_INTERLEAVED) {
        for (int i = 0; i < a->iters; i++) {
            void *p = a->alloc_fn(a->size ? a->size : MIXED_SIZES[i % MIXED_N]);
            a->free_fn(p);
        }
    } else {
        for (int i = 0; i < a->iters; i++)
            ptrs[i] = a->alloc_fn(a->size ? a->size : MIXED_SIZES[i % MIXED_N]);
        if (a->order == ORDER_FIFO)
            for (int i = 0; i < a->iters; i++)
                a->free_fn(ptrs[i]);
        else
            for (int i = a->iters - 1; i >= 0; i--)
                a->free_fn(ptrs[i]);
    }

    free(ptrs);
    return NULL;
}

/**
 * @brief Spawn MT_THREADS workers using @p alloc_fn / @p free_fn and return
 *        elapsed wall-clock milliseconds.
 *
 * @param iters     Iterations per thread.
 * @param size      Allocation size (0 = mixed).
 * @param order     Free-order strategy.
 * @param alloc_fn  Allocator function to benchmark.
 * @param free_fn   Deallocator matching @p alloc_fn.
 * @return Elapsed wall-clock time in milliseconds.
 */
static double run_mt(int iters, size_t size, free_order_t order,
                     alloc_fn_t alloc_fn, free_fn_t free_fn)
{
    pthread_t threads[MT_THREADS];
    worker_args_t args = { iters, size, order, alloc_fn, free_fn };

    prof_timer_t t;
    timer_start(&t);
    for (int i = 0; i < MT_THREADS; i++)
        pthread_create(&threads[i], NULL, mt_worker, &args);
    for (int i = 0; i < MT_THREADS; i++)
        pthread_join(threads[i], NULL);
    return timer_elapsed_ms(&t);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Run all three free-order variants of a single-threaded scenario.
 *
 * @param label_prefix  Prefix string for the report label.
 * @param iters         Number of iterations.
 * @param size          Allocation size (0 = mixed).
 * @param alloc_fn      Allocator to benchmark.
 * @param free_fn       Matching deallocator.
 * @return void
 */
#define RUN_ALL_ORDERS(label_prefix, iters, size, alloc_fn, free_fn) do { \
    report(label_prefix " interleaved", (iters),                          \
           scenario_st((iters), (size), ORDER_INTERLEAVED, alloc_fn, free_fn)); \
    report(label_prefix " FIFO", (iters),                                 \
           scenario_st((iters), (size), ORDER_FIFO,        alloc_fn, free_fn)); \
    report(label_prefix " LIFO", (iters),                                 \
           scenario_st((iters), (size), ORDER_LIFO,        alloc_fn, free_fn)); \
} while (0)

/**
 * @brief Run all three free-order variants of a multi-threaded scenario.
 *
 * @param label_prefix  Prefix string for the report label.
 * @param iters         Iterations per thread.
 * @param size          Allocation size (0 = mixed).
 * @param total         Total operations (threads × iters).
 * @param alloc_fn      Allocator to benchmark.
 * @param free_fn       Matching deallocator.
 * @return void
 */
#define RUN_MT_ALL_ORDERS(label_prefix, iters, size, total, alloc_fn, free_fn) do { \
    report(label_prefix " interleaved", (total),                               \
           run_mt((iters), (size), ORDER_INTERLEAVED, alloc_fn, free_fn));     \
    report(label_prefix " FIFO",        (total),                               \
           run_mt((iters), (size), ORDER_FIFO,        alloc_fn, free_fn));     \
    report(label_prefix " LIFO",        (total),                               \
           run_mt((iters), (size), ORDER_LIFO,        alloc_fn, free_fn));     \
} while (0)

int main(void)
{
    const int total_mt = MT_THREADS * MT_ITERS;

    printf("=== Heap Allocator Profile ===\n\n");

    /* --- mmap allocator: single-threaded --- */
    printf("mmap allocator — single-threaded (%d ops each):\n\n", ITERS);

    RUN_ALL_ORDERS("mmap  ST   16 B", ITERS, 16,         malloc, free); printf("\n");
    RUN_ALL_ORDERS("mmap  ST  256 B", ITERS, 256,        malloc, free); printf("\n");
    RUN_ALL_ORDERS("mmap  ST    4 KB", ITERS, 4096,      malloc, free); printf("\n");
    RUN_ALL_ORDERS("mmap  ST    1 MB", ITERS, 1024*1024, malloc, free); printf("\n");
    RUN_ALL_ORDERS("mmap  ST  mixed (16/128/1K/8K)", ITERS, 0, malloc, free);

    /* --- mmap allocator: multi-threaded --- */
    printf("\nmmap allocator — multi-threaded (%d threads x %d ops = %d total):\n\n",
           MT_THREADS, MT_ITERS, total_mt);

    RUN_MT_ALL_ORDERS("mmap  MT  256 B",              MT_ITERS, 256,  total_mt, malloc, free); printf("\n");
    RUN_MT_ALL_ORDERS("mmap  MT    4 KB",             MT_ITERS, 4096, total_mt, malloc, free); printf("\n");
    RUN_MT_ALL_ORDERS("mmap  MT  mixed (16/128/1K/8K)", MT_ITERS, 0, total_mt, malloc, free);

    /* --- sbrk allocator: single-threaded --- */
    printf("\nsbrk allocator — single-threaded (%d ops each):\n\n", ITERS);

    RUN_ALL_ORDERS("sbrk  ST   16 B", ITERS, 16,         sbrk_malloc, sbrk_free); printf("\n");
    RUN_ALL_ORDERS("sbrk  ST  256 B", ITERS, 256,        sbrk_malloc, sbrk_free); printf("\n");
    RUN_ALL_ORDERS("sbrk  ST    4 KB", ITERS, 4096,      sbrk_malloc, sbrk_free); printf("\n");
    RUN_ALL_ORDERS("sbrk  ST    1 MB", ITERS, 1024*1024, sbrk_malloc, sbrk_free); printf("\n");
    RUN_ALL_ORDERS("sbrk  ST  mixed (16/128/1K/8K)", ITERS, 0, sbrk_malloc, sbrk_free);

    /* --- sbrk allocator: multi-threaded --- */
    printf("\nsbrk allocator — multi-threaded (%d threads x %d ops = %d total):\n\n",
           MT_THREADS, MT_ITERS, total_mt);

    RUN_MT_ALL_ORDERS("sbrk  MT  256 B",              MT_ITERS, 256,  total_mt, sbrk_malloc, sbrk_free); printf("\n");
    RUN_MT_ALL_ORDERS("sbrk  MT    4 KB",             MT_ITERS, 4096, total_mt, sbrk_malloc, sbrk_free); printf("\n");
    RUN_MT_ALL_ORDERS("sbrk  MT  mixed (16/128/1K/8K)", MT_ITERS, 0, total_mt, sbrk_malloc, sbrk_free);

    /* --- sbrk list allocator: single-threaded --- */
    printf("\nsbrk list allocator — single-threaded (%d ops each):\n\n", ITERS);

    RUN_ALL_ORDERS("sbrk_list  ST   16 B", ITERS, 16,         sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("sbrk_list  ST  256 B", ITERS, 256,        sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("sbrk_list  ST    4 KB", ITERS, 4096,      sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("sbrk_list  ST    1 MB", ITERS, 1024*1024, sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_ALL_ORDERS("sbrk_list  ST  mixed (16/128/1K/8K)", ITERS, 0, sbrk_list_malloc, sbrk_list_free);

    /* --- sbrk list allocator: multi-threaded --- */
    printf("\nsbrk list allocator — multi-threaded (%d threads x %d ops = %d total):\n\n",
           MT_THREADS, MT_ITERS, total_mt);

    RUN_MT_ALL_ORDERS("sbrk_list  MT  256 B",              MT_ITERS, 256,  total_mt, sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_MT_ALL_ORDERS("sbrk_list  MT    4 KB",             MT_ITERS, 4096, total_mt, sbrk_list_malloc, sbrk_list_free); printf("\n");
    RUN_MT_ALL_ORDERS("sbrk_list  MT  mixed (16/128/1K/8K)", MT_ITERS, 0, total_mt, sbrk_list_malloc, sbrk_list_free);

    /* --- opt allocator: single-threaded --- */
    printf("\nopt allocator — single-threaded (%d ops each):\n\n", ITERS);

    RUN_ALL_ORDERS("opt  ST   16 B", ITERS, 16,         opt_malloc, opt_free); printf("\n");
    RUN_ALL_ORDERS("opt  ST  256 B", ITERS, 256,        opt_malloc, opt_free); printf("\n");
    RUN_ALL_ORDERS("opt  ST    4 KB", ITERS, 4096,      opt_malloc, opt_free); printf("\n");
    RUN_ALL_ORDERS("opt  ST    1 MB", ITERS, 1024*1024, opt_malloc, opt_free); printf("\n");
    RUN_ALL_ORDERS("opt  ST  mixed (16/128/1K/8K)", ITERS, 0, opt_malloc, opt_free);

    /* --- opt allocator: multi-threaded --- */
    printf("\nopt allocator — multi-threaded (%d threads x %d ops = %d total):\n\n",
           MT_THREADS, MT_ITERS, total_mt);

    RUN_MT_ALL_ORDERS("opt  MT  256 B",              MT_ITERS, 256,  total_mt, opt_malloc, opt_free); printf("\n");
    RUN_MT_ALL_ORDERS("opt  MT    4 KB",             MT_ITERS, 4096, total_mt, opt_malloc, opt_free); printf("\n");
    RUN_MT_ALL_ORDERS("opt  MT  mixed (16/128/1K/8K)", MT_ITERS, 0, total_mt, opt_malloc, opt_free);

    printf("\nDone.\n");
    return 0;
}
