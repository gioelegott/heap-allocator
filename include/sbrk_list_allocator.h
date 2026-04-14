#ifndef SBRK_LIST_ALLOCATOR_H
#define SBRK_LIST_ALLOCATOR_H

#include <stddef.h>

/**
 * @brief Allocate a block of memory using a sbrk-backed free list.
 *
 * On each call the free list is scanned for the first block that is marked
 * free and large enough to satisfy @p size (first-fit).  If a suitable block
 * is found it is reused without touching the program break.  Otherwise the
 * heap is extended via sbrk(2) and the new block is appended to the list.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void *sbrk_list_malloc(size_t size);

/**
 * @brief Free a previously allocated block.
 *
 * Marks the block as free.  If the block is at the tail of the list it is
 * removed and the program break is lowered via sbrk(2).  This reclaim
 * cascades: after removing the tail, if the new tail is also free it is
 * reclaimed too, until a live block or the empty list is reached.  Blocks
 * that are not at the tail are kept in the list for first-fit reuse.
 *
 * @param ptr Pointer previously returned by sbrk_list_malloc(). If NULL,
 *            no-op.
 * @return void
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void sbrk_list_free(void *ptr);

#endif
