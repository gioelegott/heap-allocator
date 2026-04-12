#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "timer.h"
#include "../include/allocator.h"

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
    printf("  %-52s  %8.2f ms  (%d ops, %.0f ns/op)\n",
           label, ms, iters, ms * 1e6 / iters);
}

/* ------------------------------------------------------------------ */
/* Single-threaded scenarios                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Run a single-threaded allocation scenario.
 *
 * When @p size is 0 the scenario cycles through MIXED_SIZES on each
 * iteration. The @p order parameter selects whether allocations and
 * frees are interleaved or batched (FIFO or LIFO).
 *
 * @param iters  Number of alloc/free pairs.
 * @param size   Bytes per allocation, or 0 for mixed sizes.
 * @param order  Free-order strategy.
 * @return Elapsed time in milliseconds, or -1 on allocation failure.
 */
static double scenario_st(int iters, size_t size, free_order_t order)
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
            void *p = malloc(size ? size : MIXED_SIZES[i % MIXED_N]);
            free(p);
        }
    } else {
        for (int i = 0; i < iters; i++)
            ptrs[i] = malloc(size ? size : MIXED_SIZES[i % MIXED_N]);
        if (order == ORDER_FIFO)
            for (int i = 0; i < iters; i++)
                free(ptrs[i]);
        else /* ORDER_LIFO */
            for (int i = iters - 1; i >= 0; i--)
                free(ptrs[i]);
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
    int          iters; /**< Number of alloc/free cycles to perform.     */
    size_t       size;  /**< Allocation size in bytes, or 0 for mixed.   */
    free_order_t order; /**< Free-order strategy.                        */
} worker_args_t;

/**
 * @brief Thread worker mirroring scenario_st() for concurrent use.
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
            void *p = malloc(a->size ? a->size : MIXED_SIZES[i % MIXED_N]);
            free(p);
        }
    } else {
        for (int i = 0; i < a->iters; i++)
            ptrs[i] = malloc(a->size ? a->size : MIXED_SIZES[i % MIXED_N]);
        if (a->order == ORDER_FIFO)
            for (int i = 0; i < a->iters; i++)
                free(ptrs[i]);
        else
            for (int i = a->iters - 1; i >= 0; i--)
                free(ptrs[i]);
    }

    free(ptrs);
    return NULL;
}

/**
 * @brief Spawn MT_THREADS workers and return elapsed wall-clock ms.
 *
 * @param iters  Iterations per thread.
 * @param size   Allocation size (0 = mixed).
 * @param order  Free-order strategy.
 * @return Elapsed wall-clock time in milliseconds.
 */
static double run_mt(int iters, size_t size, free_order_t order)
{
    pthread_t threads[MT_THREADS];
    worker_args_t args = { iters, size, order };

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

/** @brief Macro to run the same experiment set for all three orders. */
#define RUN_ALL_ORDERS(label_prefix, iters, size, total) do {          \
    report(label_prefix " interleaved", (iters), scenario_st((iters), (size), ORDER_INTERLEAVED)); \
    report(label_prefix " FIFO",        (iters), scenario_st((iters), (size), ORDER_FIFO));        \
    report(label_prefix " LIFO",        (iters), scenario_st((iters), (size), ORDER_LIFO));        \
} while (0)

/** @brief Macro to run the MT experiment set for all three orders. */
#define RUN_MT_ALL_ORDERS(label_prefix, iters, size, total) do {       \
    report(label_prefix " interleaved", (total), run_mt((iters), (size), ORDER_INTERLEAVED)); \
    report(label_prefix " FIFO",        (total), run_mt((iters), (size), ORDER_FIFO));        \
    report(label_prefix " LIFO",        (total), run_mt((iters), (size), ORDER_LIFO));        \
} while (0)

int main(void)
{
    const int total_mt = MT_THREADS * MT_ITERS;

    printf("=== Heap Allocator Profile ===\n\n");

    /* --- Single-threaded --- */
    printf("Single-threaded (%d ops each):\n\n", ITERS);

    RUN_ALL_ORDERS("ST   16 B", ITERS, 16,           ITERS);    printf("\n");
    RUN_ALL_ORDERS("ST  256 B", ITERS, 256,          ITERS);    printf("\n");
    RUN_ALL_ORDERS("ST    4 KB", ITERS, 4096,        ITERS);    printf("\n");
    RUN_ALL_ORDERS("ST    1 MB", ITERS, 1024*1024,   ITERS);    printf("\n");
    RUN_ALL_ORDERS("ST  mixed (16/128/1K/8K)", ITERS, 0,ITERS);

    /* --- Multi-threaded --- */
    printf("\nMulti-threaded (%d threads x %d ops = %d total):\n\n",
           MT_THREADS, MT_ITERS, total_mt);

    RUN_MT_ALL_ORDERS("MT  256 B", MT_ITERS, 256,        total_mt); printf("\n");
    RUN_MT_ALL_ORDERS("MT    4 KB", MT_ITERS, 4096,      total_mt); printf("\n");
    RUN_MT_ALL_ORDERS("MT  mixed (16/128/1K/8K)", MT_ITERS, 0, total_mt);

    printf("\nDone.\n");
    return 0;
}
