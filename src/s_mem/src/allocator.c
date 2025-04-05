
#include "s_mem/allocator.h"

static allocator_t *default_allocator = NULL;

void set_default_allocator(allocator_t *al) {
    default_allocator = al;
}

allocator_t *get_default_allocator(void) {
    return default_allocator;
}
