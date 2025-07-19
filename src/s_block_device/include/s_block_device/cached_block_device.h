
#pragma once

#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"
#include "s_data/map.h"
#include "s_util/rand.h"

typedef struct _cached_sector_entry_t {
    union {
        /**
         * Only interpret this field when this entry is in the free-list.
         * A value of cache capacity means this entry is the last in the free list.
         */
        size_t next_free;

        /**
         * Index of the sector cached here.
         * Only interpret if this entry is NOT in the free list.
         */
        size_t sector_ind;
    } i;

    /**
     * For all sector entries, this will be Non-NULL and point to a sector sized 
     * block of memory.
     */
    uint8_t *sector;
} cached_sector_entry_t;

typedef struct _cached_block_device_t {
    block_device_t super;

    allocator_t * const al;

    block_device_t * const wrapped_bd;

    /**
     * This random number generator is used when choosing which sectors to flush when the 
     * cache is full and space is needed.
     */
    rand_t r; 

    /**
     * The number of entries in the cache.
     */
    const size_t cache_cap;

    /**
     * The cache. This will be allocated in its entirety at init time.
     */
    cached_sector_entry_t * const cache;

    /**
     * Next free entry in the cache. 
     * Is cache_cap if the cache is full.
     */
    size_t next_free;

    /**
     * This is a map of sector index (size_t) -> cache index (size_t).
     * It is used to get a sectors cache entry fast.
     */
    map_t * const sector_map;
} cached_block_device_t;

/**
 * Create a new cached_block_device. 
 *
 * NOTE: This OWNs the given block device! When the cached block device is deleted, so is
 * the wrapped block device (might change this in the future)
 *
 * cc is the maximum number of sectors which can be cahced at once.
 * seed is the seed to start the prng with.
 *
 * Returns NULL if params are bad or if allocation fails.
 */
block_device_t *new_cached_block_device(allocator_t *al, block_device_t *bd, size_t cc, uint32_t seed);

static inline block_device_t *new_da_cached_block_device(block_device_t *bd, size_t cc, uint32_t seed) {
    return new_cached_block_device(get_default_allocator(), bd, cc, seed);
}

