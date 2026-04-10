#include <stdio.h>
#include "allocator.h"

int main(void)
{
    void *p = malloc(64);
    printf("allocated 64 bytes at %p\n", p);
    free(p);
    printf("freed\n");
    return 0;
}
