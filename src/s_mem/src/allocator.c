
#include "s_mem/allocator.h"

static allocator_t *default_allocator = NULL;

allocator_t *set_default_allocator(allocator_t *al) {
    allocator_t *ret = default_allocator;
    default_allocator = al;

    return ret;
}

allocator_t *get_default_allocator(void) {
    return default_allocator;
}
