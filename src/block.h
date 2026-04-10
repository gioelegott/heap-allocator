#ifndef BLOCK_H
#define BLOCK_H

#include <stddef.h>

/**
 * @brief Metadata header prepended to every allocated region.
 *
 * @struct block_header
 *
 * Layout in memory (low address to high address):
 * @code
 * [ block_header_t | <usable bytes> ]
 * ^                ^
 * mmap base        pointer returned to caller
 * @endcode
 *
 * malloc() writes this header immediately before the pointer it returns.
 * free() reads it back by subtracting sizeof(block_header_t) from the
 * caller's pointer, recovering the exact region size needed for munmap().
 */
typedef struct {
    size_t size; /**< Usable bytes available to the caller, excluding this header. */
} block_header_t;

#endif
