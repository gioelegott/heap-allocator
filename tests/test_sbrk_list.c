#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
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
 * @brief Verify that a freed non-tail block is reused by the next allocation.
 *
 * Allocates two blocks (first and sentinel), frees first (which is not at
 * the tail), then allocates again with the same size. The second allocation
 * must return the same pointer as first (list reuse, not a new sbrk region).
 */
static void test_sbrk_list_free_reuses_block(void)
{
    void *first    = sbrk_list_malloc(64);
    void *sentinel = sbrk_list_malloc(64); /* keeps first off the tail */
    assert(first != NULL && sentinel != NULL);

    sbrk_list_free(first); /* non-tail free — stays in list */

    void *second = sbrk_list_malloc(64);
    assert(second == first); /* must reuse first's slot */

    sbrk_list_free(second);
    sbrk_list_free(sentinel);
    printf("PASS test_sbrk_list_free_reuses_block\n");
}

/**
 * @brief Verify that a non-top freed block is reused (FIFO order).
 *
 * Allocates two blocks A and B, frees A (which is not at the tail),
 * then allocates again. The allocator must reuse A rather than extending
 * the heap.
 */
static void test_sbrk_list_free_fifo_reuses_block(void)
{
    void *a = sbrk_list_malloc(64);
    void *b = sbrk_list_malloc(64);
    assert(a != NULL && b != NULL);

    sbrk_list_free(a); /* free the non-tail block */

    void *c = sbrk_list_malloc(64);
    assert(c == a); /* must reuse a, not extend the heap */

    sbrk_list_free(c);
    sbrk_list_free(b);
    printf("PASS test_sbrk_list_free_fifo_reuses_block\n");
}

/**
 * @brief Verify that a larger freed non-tail block satisfies a smaller request.
 *
 * Allocates a 128-byte block followed by a sentinel, frees the 128-byte
 * block (non-tail), then requests only 64 bytes. The allocator must reuse
 * the 128-byte slot (first-fit) rather than extending the heap.
 */
static void test_sbrk_list_reuse_larger_block(void)
{
    void *big      = sbrk_list_malloc(128);
    void *sentinel = sbrk_list_malloc(64); /* keeps big off the tail */
    assert(big != NULL && sentinel != NULL);

    sbrk_list_free(big); /* non-tail free — stays in list */

    void *small = sbrk_list_malloc(64);
    assert(small == big); /* first-fit must match the 128-byte slot */

    sbrk_list_free(small);
    sbrk_list_free(sentinel);
    printf("PASS test_sbrk_list_reuse_larger_block\n");
}

/**
 * @brief Verify that freeing the last block lowers the program break.
 *
 * Allocates a block, records the break, frees the block (it is the tail),
 * then checks that the break has moved down.
 */
static void test_sbrk_list_free_last_lowers_break(void)
{
    void *p = sbrk_list_malloc(64);
    assert(p != NULL);

    void *brk_before = sbrk(0);
    sbrk_list_free(p); /* tail block — must be reclaimed */
    assert(sbrk(0) < brk_before);

    printf("PASS test_sbrk_list_free_last_lowers_break\n");
}

/**
 * @brief Verify that cascade reclaim removes consecutive free tail blocks.
 *
 * Allocates A then B, frees A (non-tail, stays in list), then frees B
 * (tail, reclaimed). After B is removed the new tail (A) is also free and
 * must be reclaimed too, leaving the list empty and the break at its
 * original position.
 */
static void test_sbrk_list_cascade_reclaim(void)
{
    void *brk_start = sbrk(0);

    void *a = sbrk_list_malloc(64);
    void *b = sbrk_list_malloc(64);
    assert(a != NULL && b != NULL);

    sbrk_list_free(a); /* non-tail — stays in list as free */
    sbrk_list_free(b); /* tail — triggers cascade: reclaims B then A */

    assert(sbrk(0) == brk_start); /* break must be fully restored */
    printf("PASS test_sbrk_list_cascade_reclaim\n");
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
    test_sbrk_list_free_last_lowers_break();
    test_sbrk_list_cascade_reclaim();
    printf("All sbrk list tests passed.\n");
    return 0;
}
