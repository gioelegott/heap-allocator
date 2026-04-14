#define _GNU_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include "block.h"
#include "sbrk_lock.h"
#include "../include/sbrk_allocator.h"

/** @brief File-local mutex protecting the program break (conditionally compiled). */
DECLARE_SBRK_MUTEX

/**
 * @brief Allocate a block of memory of the given size using sbrk.
 *
 * Extends the program break by sizeof(block_header_t) + @p size bytes via
 * sbrk(2), prepending a block_header_t that records the usable size so that
 * sbrk_free() can reclaim the region when it sits at the top of the heap.
 * The program break is modified under a mutex when THREAD_SAFE=1.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void *sbrk_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t total = sizeof(block_header_t) + size;
    SBRK_LOCK();
    void *region = sbrk((intptr_t)total);
    void *result = NULL;
    if (region != (void *)-1) {
        block_header_t *hdr = region;
        hdr->size = size;
        result = hdr + 1;
    }
    SBRK_UNLOCK();
    return result;
}

/**
 * @brief Free a previously allocated block of memory.
 *
 * Recovers the block_header_t preceding @p ptr to determine the total size.
 * If the block is at the top of the heap the program break is lowered by
 * sbrk(2); otherwise the block is abandoned (no reuse).  The check and
 * the conditional sbrk call are performed under a mutex when THREAD_SAFE=1.
 *
 * @param ptr Pointer previously returned by sbrk_malloc(). If NULL, no-op.
 * @return void
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void sbrk_free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_header_t *hdr = (block_header_t *)ptr - 1;
    size_t total = sizeof(block_header_t) + hdr->size;

    SBRK_LOCK();
    /* Only reclaim if this block is at the top of the heap. */
    void *top = sbrk(0);
    if ((char *)hdr + total == (char *)top)
        sbrk(-(intptr_t)total);
    SBRK_UNLOCK();
}
