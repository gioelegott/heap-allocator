#ifndef SBRK_ALLOCATOR_H
#define SBRK_ALLOCATOR_H

#include <stddef.h>

/**
 * @brief Allocate a block of memory of the given size using sbrk.
 *
 * Extends the program break by sizeof(block_header_t) + @p size bytes via
 * sbrk(2), prepending a block_header_t that records the usable size so that
 * sbrk_free() can reclaim the region when it sits at the top of the heap.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Not thread-safe: the program break is a process-wide resource.
 */
void *sbrk_malloc(size_t size);

/**
 * @brief Free a previously allocated block of memory.
 *
 * Recovers the block_header_t preceding @p ptr to determine the total size.
 * If the block is at the top of the heap the program break is lowered by
 * sbrk(2); otherwise the block is abandoned (no reuse).
 *
 * @param ptr Pointer previously returned by sbrk_malloc(). If NULL, no-op.
 * @return void
 * @note Not thread-safe: the program break is a process-wide resource.
 */
void sbrk_free(void *ptr);

#endif
