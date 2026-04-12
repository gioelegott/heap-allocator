#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "sbrk_allocator.h"

/**
 * @brief Verify that sbrk_malloc returns a non-NULL pointer for a positive size.
 */
static void test_sbrk_malloc_returns_non_null(void)
{
    void *p = sbrk_malloc(16);
    assert(p != NULL);
    sbrk_free(p);
    printf("PASS test_sbrk_malloc_returns_non_null\n");
}

/**
 * @brief Verify that sbrk_malloc(0) returns NULL.
 */
static void test_sbrk_malloc_zero_returns_null(void)
{
    void *p = sbrk_malloc(0);
    assert(p == NULL);
    printf("PASS test_sbrk_malloc_zero_returns_null\n");
}

/**
 * @brief Verify that allocated memory is writable and readable.
 */
static void test_sbrk_malloc_memory_is_usable(void)
{
    const size_t n = 256;
    char *buf = sbrk_malloc(n);
    assert(buf != NULL);
    memset(buf, 0xAB, n);
    for (size_t i = 0; i < n; i++)
        assert((unsigned char)buf[i] == 0xAB);
    sbrk_free(buf);
    printf("PASS test_sbrk_malloc_memory_is_usable\n");
}

/**
 * @brief Verify that two successive sbrk_malloc calls return distinct pointers.
 */
static void test_sbrk_malloc_distinct_pointers(void)
{
    void *a = sbrk_malloc(32);
    void *b = sbrk_malloc(32);
    assert(a != NULL && b != NULL);
    assert(a != b);
    sbrk_free(b);
    sbrk_free(a);
    printf("PASS test_sbrk_malloc_distinct_pointers\n");
}

/**
 * @brief Verify that sbrk_free(NULL) does not crash.
 */
static void test_sbrk_free_null_is_noop(void)
{
    sbrk_free(NULL);
    printf("PASS test_sbrk_free_null_is_noop\n");
}

/**
 * @brief Verify that a large allocation succeeds and is usable.
 */
static void test_sbrk_malloc_large(void)
{
    const size_t n = 1024 * 1024; /* 1 MiB */
    char *buf = sbrk_malloc(n);
    assert(buf != NULL);
    buf[0] = 1;
    buf[n - 1] = 2;
    assert(buf[0] == 1 && buf[n - 1] == 2);
    sbrk_free(buf);
    printf("PASS test_sbrk_malloc_large\n");
}

/**
 * @brief Verify that LIFO sbrk_free lowers the program break.
 *
 * Allocates two blocks, then frees them in reverse (LIFO) order.
 * After each free the program break must be strictly lower than before.
 */
static void test_sbrk_free_lifo_reclaims_heap(void)
{
    void *a = sbrk_malloc(64);
    void *b = sbrk_malloc(64);
    assert(a != NULL && b != NULL);

    void *brk_before = sbrk(0);
    sbrk_free(b);
    assert(sbrk(0) < brk_before);  /* break must have moved down */

    brk_before = sbrk(0);
    sbrk_free(a);
    assert(sbrk(0) < brk_before);

    printf("PASS test_sbrk_free_lifo_reclaims_heap\n");
}

/**
 * @brief Verify that FIFO sbrk_free (non-top block) does not lower the break.
 *
 * Allocates two blocks then frees the first (non-top) one. The program
 * break must not change because that block is not at the top of the heap.
 */
static void test_sbrk_free_fifo_does_not_reclaim(void)
{
    void *a = sbrk_malloc(64);
    void *b = sbrk_malloc(64);
    assert(a != NULL && b != NULL);

    void *brk_before = sbrk(0);
    sbrk_free(a);                   /* non-top block — break must stay */
    assert(sbrk(0) == brk_before);

    sbrk_free(b);                   /* now top — reclaimed */
    printf("PASS test_sbrk_free_fifo_does_not_reclaim\n");
}

int main(void)
{
    test_sbrk_malloc_returns_non_null();
    test_sbrk_malloc_zero_returns_null();
    test_sbrk_malloc_memory_is_usable();
    test_sbrk_malloc_distinct_pointers();
    test_sbrk_free_null_is_noop();
    test_sbrk_malloc_large();
    test_sbrk_free_lifo_reclaims_heap();
    test_sbrk_free_fifo_does_not_reclaim();
    printf("All sbrk tests passed.\n");
    return 0;
}
