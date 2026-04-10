#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "allocator.h"

/**
 * @brief Verify that malloc returns a non-NULL pointer for a positive size.
 */
static void test_malloc_returns_non_null(void)
{
    void *p = malloc(16);
    assert(p != NULL);
    free(p);
    printf("PASS test_malloc_returns_non_null\n");
}

/**
 * @brief Verify that malloc(0) returns NULL.
 */
static void test_malloc_zero_returns_null(void)
{
    void *p = malloc(0);
    assert(p == NULL);
    printf("PASS test_malloc_zero_returns_null\n");
}

/**
 * @brief Verify that allocated memory is writable and readable.
 */
static void test_malloc_memory_is_usable(void)
{
    const size_t n = 256;
    char *buf = malloc(n);
    assert(buf != NULL);
    memset(buf, 0xAB, n);
    for (size_t i = 0; i < n; i++)
        assert((unsigned char)buf[i] == 0xAB);
    free(buf);
    printf("PASS test_malloc_memory_is_usable\n");
}

/**
 * @brief Verify that two successive malloc calls return distinct pointers.
 */
static void test_malloc_distinct_pointers(void)
{
    void *a = malloc(32);
    void *b = malloc(32);
    assert(a != NULL && b != NULL);
    assert(a != b);
    free(a);
    free(b);
    printf("PASS test_malloc_distinct_pointers\n");
}

/**
 * @brief Verify that free(NULL) does not crash.
 */
static void test_free_null_is_noop(void)
{
    free(NULL);
    printf("PASS test_free_null_is_noop\n");
}

/**
 * @brief Verify that a large allocation succeeds and is usable.
 */
static void test_malloc_large(void)
{
    const size_t n = 1024 * 1024; /* 1 MiB */
    char *buf = malloc(n);
    assert(buf != NULL);
    buf[0] = 1;
    buf[n - 1] = 2;
    assert(buf[0] == 1 && buf[n - 1] == 2);
    free(buf);
    printf("PASS test_malloc_large\n");
}

int main(void)
{
    test_malloc_returns_non_null();
    test_malloc_zero_returns_null();
    test_malloc_memory_is_usable();
    test_malloc_distinct_pointers();
    test_free_null_is_noop();
    test_malloc_large();
    printf("All basic tests passed.\n");
    return 0;
}
