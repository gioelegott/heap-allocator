#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

/**
 * @brief Allocate a block of memory of the given size.
 *
 * Each call maps a new anonymous private region via mmap, prepending a
 * block_header_t that records the usable size so that free() can unmap
 * the exact region later.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Thread-safe: each call to malloc() issues an independent mmap()
 *       syscall and mutates no shared allocator state, so no lock is needed.
 */
void *malloc(size_t size);

/**
 * @brief Free a previously allocated block of memory.
 *
 * Recovers the block_header_t preceding @p ptr to determine the total
 * mapped size, then unmaps the entire region with munmap().
 *
 * @param ptr Pointer previously returned by malloc(). Passing NULL is
 *            safe and has no effect.
 * @return void
 * @note Thread-safe: each call to free() unmaps an independent mmap region
 *       and touches no shared allocator state.
 */
void  free(void *ptr);

#endif
