#define _GNU_SOURCE
#include <stddef.h>
#include "sbrk_lock.h"
#include "../include/opt_allocator.h"

/** @brief File-local mutex (conditionally compiled). */
DECLARE_SBRK_MUTEX

/**
 * @brief Allocate @p size bytes of memory.
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

    SBRK_LOCK();

    /* TODO: implement */

    SBRK_UNLOCK();
    return NULL;
}

/**
 * @brief Free a previously allocated block.
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

    /* TODO: implement */

    SBRK_UNLOCK();
}
