#define _GNU_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include "sbrk_lock.h"
#include "../include/opt_allocator.h"

/** @brief File-local mutex (conditionally compiled). */
DECLARE_SBRK_MUTEX

/**
 * @brief Header prepended to every allocation in the heap.
 *
 * Blocks are laid out contiguously in the heap:
 * @code
 *  low address                          high address
 *  +-----------------------+---------------------------+
 *  |   block_header_t      |   usable bytes            |
 *  |   (size, free)        |   (returned to caller)    |
 *  +-----------------------+---------------------------+
 * @endcode
 * Traversal advances by @c sizeof(block_header_t) + @c size bytes.
 */
typedef struct block_header {
    size_t size; /**< Usable bytes available to the caller.       */
    int    free; /**< 1 if this block may be reused, 0 otherwise. */
} block_header_t;

/** @brief Start of the implicit free list. NULL until the first allocation. */
static block_header_t *head = NULL;


/**
 * @brief Allocate @p size bytes of memory.
 *
 * Performs a first-fit search over the implicit free list.  Adjacent free
 * blocks are coalesced on-the-fly.  If no suitable block is found the heap
 * is extended via sbrk(2).  The returned block is split when the remainder
 * would accommodate at least one usable byte beyond the header.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void *opt_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    /* Round up to the alignment of block_header_t so every header stays
     * naturally aligned after a split. */
    size = (size + _Alignof(block_header_t) - 1)
           & ~(_Alignof(block_header_t) - 1);

    size_t total = sizeof(block_header_t) + size;

    SBRK_LOCK();

    /* Snapshot the current program break; used as the end-of-heap sentinel. */
    char *heap_end = sbrk(0);
    if (heap_end == (char *)-1) {
        SBRK_UNLOCK();
        return NULL;
    }

    /* On the very first call, anchor head to the start of the heap. */
    if (head == NULL)
        head = (block_header_t *)heap_end;

    /* --- First-fit search with forward coalescing --- */
    char           *ptr = (char *)head;
    block_header_t *hdr = head;

    while (ptr < heap_end) {
        if (!hdr->free) {
            /* Block is live — skip it. */
            ptr += sizeof(block_header_t) + hdr->size;
        } else {
            /* Block is free — try to coalesce with consecutive free neighbours
             * until the merged block is large enough or a live block/end is hit. */
            char           *next_ptr = ptr + sizeof(block_header_t) + hdr->size;
            block_header_t *next_hdr = (block_header_t *)next_ptr;

            while (hdr->size < size
                   && next_ptr < heap_end
                   && next_hdr->free) {
                /* Absorb next_hdr into hdr (reclaim its header overhead too). */
                hdr->size += sizeof(block_header_t) + next_hdr->size;
                next_ptr  += sizeof(block_header_t) + next_hdr->size;
                next_hdr   = (block_header_t *)next_ptr;
            }

            if (hdr->size >= size) {
                /* This block (possibly after coalescing) is large enough. */
                hdr->free = 0;

                /* Split only when the remainder can hold a header + ≥1 byte. */
                if (hdr->size > size + sizeof(block_header_t)) {
                    block_header_t *rem = (block_header_t *)(ptr + sizeof(block_header_t) + size);
                    rem->free = 1;
                    rem->size = hdr->size - size - sizeof(block_header_t);
                    hdr->size = size;
                }

                SBRK_UNLOCK();
                return ptr + sizeof(block_header_t);
            }

            /* Still too small after coalescing — advance past this block. */
            ptr += sizeof(block_header_t) + hdr->size;
        }
        hdr = (block_header_t *)ptr;
    }

    /* --- No suitable free block found: extend the heap --- */
    void *region = sbrk((intptr_t)total);
    if (region == (void *)-1) {
        SBRK_UNLOCK();
        return NULL;
    }

    hdr        = (block_header_t *)ptr;
    hdr->size  = size;
    hdr->free  = 0;

    SBRK_UNLOCK();
    return ptr + sizeof(block_header_t);
}

/**
 * @brief Free a previously allocated block.
 *
 * Marks the block as free so it can be reused by a future opt_malloc() call.
 * Deferred coalescing is performed lazily during the next opt_malloc() search.
 *
 * @param ptr Pointer previously returned by opt_malloc(). If NULL, no-op.
 * @return void
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void opt_free(void *ptr)
{
    if (ptr == NULL)
        return;

    SBRK_LOCK();

    block_header_t *hdr = (block_header_t *)ptr - 1;
    hdr->free = 1;

    SBRK_UNLOCK();
}
