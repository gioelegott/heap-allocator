#include <sys/mman.h>
#include <stddef.h>
#include "block.h"
#include "../include/allocator.h"

/**
 * @brief Allocate a block of memory of the given size.
 *
 * Each call maps a new anonymous private region via mmap, prepending a
 * block_header_t that records the usable size so that free() can unmap
 * the exact region later.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the usable memory region, or NULL on failure.
 */
void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t total = sizeof(block_header_t) + size;
    void *region = mmap(NULL, total,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (region == MAP_FAILED)
        return NULL;

    block_header_t *hdr = region;
    hdr->size = size;
    return hdr + 1;
}

/**
 * @brief Free a previously allocated block of memory.
 *
 * Recovers the block_header_t preceding @p ptr to determine the total
 * mapped size, then unmaps the entire region with munmap.
 *
 * @param ptr Pointer previously returned by malloc(). If NULL, no-op.
 */
void free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_header_t *hdr = (block_header_t *)ptr - 1;
    size_t total = sizeof(block_header_t) + hdr->size;
    munmap(hdr, total);
}
