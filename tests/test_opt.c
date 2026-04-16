#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "opt_allocator.h"

/**
 * @brief Verify that opt_malloc returns a non-NULL pointer for a positive size.
 */
static void test_opt_malloc_returns_non_null(void)
{
    void *p = opt_malloc(16);
    assert(p != NULL);
    opt_free(p);
    printf("PASS test_opt_malloc_returns_non_null\n");
}

/**
 * @brief Verify that opt_malloc(0) returns NULL.
 */
static void test_opt_malloc_zero_returns_null(void)
{
    void *p = opt_malloc(0);
    assert(p == NULL);
    printf("PASS test_opt_malloc_zero_returns_null\n");
}

/**
 * @brief Verify that allocated memory is writable and readable.
 */
static void test_opt_malloc_memory_is_usable(void)
{
    const size_t n = 256;
    char *buf = opt_malloc(n);
    assert(buf != NULL);
    memset(buf, 0xAB, n);
    for (size_t i = 0; i < n; i++)
        assert((unsigned char)buf[i] == 0xAB);
    opt_free(buf);
    printf("PASS test_opt_malloc_memory_is_usable\n");
}

/**
 * @brief Verify that two live allocations return distinct pointers.
 */
static void test_opt_malloc_distinct_pointers(void)
{
    void *a = opt_malloc(32);
    void *b = opt_malloc(32);
    assert(a != NULL && b != NULL);
    assert(a != b);
    opt_free(a);
    opt_free(b);
    printf("PASS test_opt_malloc_distinct_pointers\n");
}

/**
 * @brief Verify that opt_free(NULL) does not crash.
 */
static void test_opt_free_null_is_noop(void)
{
    opt_free(NULL);
    printf("PASS test_opt_free_null_is_noop\n");
}

/**
 * @brief Verify that a large allocation succeeds and is usable.
 */
static void test_opt_malloc_large(void)
{
    const size_t n = 1024 * 1024; /* 1 MiB */
    char *buf = opt_malloc(n);
    assert(buf != NULL);
    buf[0] = 1;
    buf[n - 1] = 2;
    assert(buf[0] == 1 && buf[n - 1] == 2);
    opt_free(buf);
    printf("PASS test_opt_malloc_large\n");
}

/**
 * @brief Verify that a freed block is reused by the next same-size allocation.
 *
 * Allocates two blocks (first and sentinel), frees first, then allocates
 * again with the same size.  The sentinel prevents first from being reclaimed
 * as a tail block.  Reuse is confirmed by checking that the program break did
 * not advance after the second allocation (no new sbrk extension was needed).
 */
static void test_opt_free_reuses_block(void)
{
    void *first    = opt_malloc(64);
    void *sentinel = opt_malloc(64);
    assert(first != NULL && sentinel != NULL);

    opt_free(first);

    void *break_before = sbrk(0);
    void *second = opt_malloc(64);
    assert(second != NULL);
    /* Heap must not have grown: second came from the freed block, not sbrk. */
    assert(sbrk(0) == break_before);
    printf("Allocated first block at %p, second block at %p\n", first, second);

    opt_free(second);
    opt_free(sentinel);
    printf("PASS test_opt_free_reuses_block\n");
}

int main(void)
{
    test_opt_malloc_zero_returns_null();
    test_opt_free_null_is_noop();
    test_opt_malloc_returns_non_null();
    test_opt_malloc_memory_is_usable();
    test_opt_malloc_distinct_pointers();
    test_opt_malloc_large();
    test_opt_free_reuses_block();
    printf("All opt allocator tests passed.\n");
    return 0;
}
