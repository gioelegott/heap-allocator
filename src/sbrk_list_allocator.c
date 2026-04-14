#define _GNU_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include "sbrk_lock.h"
#include "../include/sbrk_list_allocator.h"

/* ------------------------------------------------------------------ */
/* Internal block node                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Intrusive list node prepended to every heap region.
 *
 * Layout in memory (low address → high address):
 * @code
 * [ block_node_t | <usable bytes> ]
 * ^               ^
 * sbrk base       pointer returned to caller
 * @endcode
 *
 * All nodes form a singly-linked list rooted at @c head.  The list is never
 * reordered; new nodes are always appended at the tail.
 */
typedef struct block_node {
    struct block_node *next; /**< Next node in the list, or NULL if last.      */
    size_t             size; /**< Usable bytes available to the caller.        */
    int                free; /**< 1 if this block may be reused, 0 otherwise.  */
} block_node_t;

/** @brief Head of the global free list. NULL until the first allocation. */
static block_node_t *head = NULL;

/** @brief File-local mutex protecting @c head and the program break (conditionally compiled). */
DECLARE_SBRK_MUTEX

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Allocate a block of memory using a sbrk-backed free list.
 *
 * On each call the free list is scanned for the first block that is marked
 * free and large enough to satisfy @p size (first-fit).  If a suitable block
 * is found it is reused without touching the program break.  Otherwise the
 * heap is extended via sbrk(2) and the new block is appended to the list.
 * The entire scan and any heap extension are performed under a mutex when
 * THREAD_SAFE=1.
 *
 * @param size Number of usable bytes requested. If 0, returns NULL.
 * @return Pointer to the start of the usable memory region, or NULL on
 *         failure or when @p size is 0.
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void *sbrk_list_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    SBRK_LOCK();

    /* First-fit search: reuse the first free block large enough. */
    block_node_t *node = head;
    while (node != NULL) {
        if (node->free && node->size >= size) {
            node->free = 0;
            SBRK_UNLOCK();
            return node + 1;
        }
        node = node->next;
    }

    /* No suitable free block found — extend the heap. */
    size_t total = sizeof(block_node_t) + size;
    block_node_t *new_node = sbrk((intptr_t)total);
    if (new_node == (void *)-1) {
        SBRK_UNLOCK();
        return NULL;
    }

    new_node->size = size;
    new_node->free = 0;
    new_node->next = NULL;

    /* Append to the tail of the list. */
    if (head == NULL) {
        head = new_node;
    } else {
        block_node_t *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        tail->next = new_node;
    }

    SBRK_UNLOCK();
    return new_node + 1;
}

/**
 * @brief Free a previously allocated block.
 *
 * Marks the block as free.  If the freed block is at the tail of the list,
 * it is removed from the list and the program break is lowered via sbrk(2).
 * This reclaim cascades: after removing the tail, if the new tail is also
 * free it is reclaimed too, and so on, until a live block is reached or the
 * list is empty.  The entire operation is performed under a mutex when
 * THREAD_SAFE=1.
 *
 * Blocks that are not at the tail are kept in the list and made available
 * for reuse by a future sbrk_list_malloc() call.
 *
 * @param ptr Pointer previously returned by sbrk_list_malloc(). If NULL,
 *            no-op.
 * @return void
 * @note Thread-safe when compiled with THREAD_SAFE=1 (the default);
 *       not thread-safe when THREAD_SAFE=0.
 */
void sbrk_list_free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_node_t *node = (block_node_t *)ptr - 1;

    SBRK_LOCK();
    node->free = 1;

    /* Cascade: reclaim consecutive free blocks from the tail of the list. */
    while (head != NULL) {
        /* Walk to the tail, tracking the predecessor. */
        block_node_t *prev = NULL;
        block_node_t *tail = head;
        while (tail->next != NULL) {
            prev = tail;
            tail = tail->next;
        }

        if (!tail->free)
            break;

        /* Unlink the tail and lower the program break. */
        if (prev != NULL)
            prev->next = NULL;
        else
            head = NULL;

        sbrk(-(intptr_t)(sizeof(block_node_t) + tail->size));
    }
    SBRK_UNLOCK();
}
