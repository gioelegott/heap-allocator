#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sbrk_list_allocator.h"

/**
 * @brief Verify that sbrk_list_malloc returns a non-NULL pointer for a positive size.
 */
static void test_sbrk_list_malloc_returns_non_null(void)
{
    void *p = sbrk_list_malloc(16);
    assert(p != NULL);
    sbrk_list_free(p);
    printf("PASS test_sbrk_list_malloc_returns_non_null\n");
}

/**
 * @brief Verify that sbrk_list_malloc(0) returns NULL.
 */
static void test_sbrk_list_malloc_zero_returns_null(void)
{
    void *p = sbrk_list_malloc(0);
    assert(p == NULL);
    printf("PASS test_sbrk_list_malloc_zero_returns_null\n");
}

/**
 * @brief Verify that allocated memory is writable and readable.
 */
static void test_sbrk_list_malloc_memory_is_usable(void)
{
    const size_t n = 256;
    char *buf = sbrk_list_malloc(n);
    assert(buf != NULL);
    memset(buf, 0xAB, n);
    for (size_t i = 0; i < n; i++)
        assert((unsigned char)buf[i] == 0xAB);
    sbrk_list_free(buf);
    printf("PASS test_sbrk_list_malloc_memory_is_usable\n");
}

/**
 * @brief Verify that two live allocations return distinct pointers.
 */
static void test_sbrk_list_malloc_distinct_pointers(void)
{
    void *a = sbrk_list_malloc(32);
    void *b = sbrk_list_malloc(32);
    assert(a != NULL && b != NULL);
    assert(a != b);
    sbrk_list_free(a);
    sbrk_list_free(b);
    printf("PASS test_sbrk_list_malloc_distinct_pointers\n");
}

/**
 * @brief Verify that sbrk_list_free(NULL) does not crash.
 */
static void test_sbrk_list_free_null_is_noop(void)
{
    sbrk_list_free(NULL);
    printf("PASS test_sbrk_list_free_null_is_noop\n");
}

/**
 * @brief Verify that a large allocation succeeds and is usable.
 */
static void test_sbrk_list_malloc_large(void)
{
    const size_t n = 1024 * 1024; /* 1 MiB */
    char *buf = sbrk_list_malloc(n);
    assert(buf != NULL);
    buf[0] = 1;
    buf[n - 1] = 2;
    assert(buf[0] == 1 && buf[n - 1] == 2);
    sbrk_list_free(buf);
    printf("PASS test_sbrk_list_malloc_large\n");
}

/**
 * @brief Verify that a freed block is reused by the next allocation of the same size.
 *
 * Allocates a block, frees it, then allocates again with the same size.
 * The second allocation must return the same pointer (block was reused, not
 * a new sbrk region).
 */
static void test_sbrk_list_free_reuses_block(void)
{
    void *first = sbrk_list_malloc(64);
    assert(first != NULL);
    sbrk_list_free(first);

    void *second = sbrk_list_malloc(64);
    assert(second == first); /* must reuse the freed block */
    sbrk_list_free(second);
    printf("PASS test_sbrk_list_free_reuses_block\n");
}

/**
 * @brief Verify that a non-top freed block is reused (FIFO order).
 *
 * Allocates two blocks A and B, frees A (which is not at the top of the
 * heap), then allocates again.  The allocator must reuse A rather than
 * extending the heap.
 */
static void test_sbrk_list_free_fifo_reuses_block(void)
{
    void *a = sbrk_list_malloc(64);
    void *b = sbrk_list_malloc(64);
    assert(a != NULL && b != NULL);

    sbrk_list_free(a); /* free the non-top block */

    void *c = sbrk_list_malloc(64);
    assert(c == a); /* must reuse a, not extend the heap */

    sbrk_list_free(c);
    sbrk_list_free(b);
    printf("PASS test_sbrk_list_free_fifo_reuses_block\n");
}

/**
 * @brief Verify that a larger freed block satisfies a smaller request.
 *
 * Allocates a 128-byte block, frees it, then requests only 64 bytes.
 * The allocator must reuse the 128-byte slot (first-fit) rather than
 * extending the heap.
 */
static void test_sbrk_list_reuse_larger_block(void)
{
    void *big = sbrk_list_malloc(128);
    assert(big != NULL);
    sbrk_list_free(big);

    void *small = sbrk_list_malloc(64);
    assert(small == big); /* first-fit must match the 128-byte slot */
    sbrk_list_free(small);
    printf("PASS test_sbrk_list_reuse_larger_block\n");
}

int main(void)
{
    test_sbrk_list_malloc_returns_non_null();
    test_sbrk_list_malloc_zero_returns_null();
    test_sbrk_list_malloc_memory_is_usable();
    test_sbrk_list_malloc_distinct_pointers();
    test_sbrk_list_free_null_is_noop();
    test_sbrk_list_malloc_large();
    test_sbrk_list_free_reuses_block();
    test_sbrk_list_free_fifo_reuses_block();
    test_sbrk_list_reuse_larger_block();
    printf("All sbrk list tests passed.\n");
    return 0;
}
