
#pragma once

#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"

typedef struct _mem_block_device_t {
    block_device_t super;

    allocator_t * const al;
    
    const size_t sector_size;
    const size_t num_sectors;
    void * const mem;
} mem_block_device_t;

/**
 * Create a new memory block device.
 *
 * Returns NULL if any of the arguments are NULL or zero.
 * Returns NULL if there is a failure to allocate.
 */
block_device_t *new_mem_block_device(allocator_t *al, size_t ss, size_t ns);
