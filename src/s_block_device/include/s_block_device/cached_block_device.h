
#pragma once

#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"
#include "s_data/map.h"

typedef struct _cached_sector_pair_t {
    size_t sector_ind;
    uint8_t *sector;
} cached_sector_pair_t;

typedef struct _cached_block_device_t {
    block_device_t super;

    allocator_t * const al;

    block_device_t * const wrapped_bd;

    /**
     * The number of entries in the cache.
     */
    const size_t cache_cap;

    /**
     * The cache. This will be allocated in its entirety at init time.
     */
    cached_sector_pair_t * const cache;

    /**
     * This is a map of sector index (size_t) -> cache index (size_t).
     * It is used to get a sectors cache entry fast.
     */
    map_t * const sector_map;
} cached_block_device_t;

