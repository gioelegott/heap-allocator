#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "sbrk_allocator.h"

#define NUM_THREADS  8
#define ALLOCS_EACH  64
#define ALLOC_SIZE   128

/**
 * @brief Thread worker: performs multiple sbrk_malloc/write/sbrk_free cycles.
 *
 * Each iteration allocates a buffer, fills it with a known byte pattern,
 * verifies the pattern is intact, then frees the buffer. Any corruption
 * from unsynchronised access will trip the assert.
 *
 * @param arg Unused.
 * @return NULL always.
 */
static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ALLOCS_EACH; i++) {
        char *buf = sbrk_malloc(ALLOC_SIZE);
        assert(buf != NULL);
        memset(buf, (unsigned char)i, ALLOC_SIZE);
        for (int j = 0; j < ALLOC_SIZE; j++)
            assert((unsigned char)buf[j] == (unsigned char)i);
        sbrk_free(buf);
    }
    return NULL;
}

/**
 * @brief Spawn NUM_THREADS threads that each run worker(), then join them all.
 *
 * Verifies that concurrent sbrk_malloc/sbrk_free calls from multiple threads
 * do not produce data corruption or crashes when THREAD_SAFE=1.
 */
static void test_sbrk_concurrent_alloc_free(void)
{
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        assert(pthread_create(&threads[i], NULL, worker, NULL) == 0);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("PASS test_sbrk_concurrent_alloc_free\n");
}

int main(void)
{
    test_sbrk_concurrent_alloc_free();
    printf("All sbrk thread tests passed.\n");
    return 0;
}
