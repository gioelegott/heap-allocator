#ifndef BLOCK_H
#define BLOCK_H

#include <stddef.h>

typedef struct {
    size_t size; /* usable bytes, excluding this header */
} block_header_t;

#endif
