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
 * @note Not thread-safe: the free list and program break are process-wide
 *       resources with no locking.
 */
void *sbrk_list_malloc(size_t size);

/**
 * @brief Free a previously allocated block.
 *
 * Marks the block as free in the list so that a future sbrk_list_malloc()
 * call can reuse it.  The program break is never lowered; memory is only
 * reclaimed logically inside the process.
 *
 * @param ptr Pointer previously returned by sbrk_list_malloc(). If NULL,
 *            no-op.
 * @return void
 * @note Not thread-safe: the free list is a process-wide resource with no
 *       locking.
 */
void sbrk_list_free(void *ptr);

#endif
