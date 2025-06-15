
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "s_util/err.h"

typedef struct _block_device_t block_device_t;

typedef struct _block_device_impl_t {
    size_t (*bd_num_sectors)(block_device_t *bd);
    size_t (*bd_sector_size)(block_device_t *bd);

    fernos_error_t (*bd_read)(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest);
    fernos_error_t (*bd_write)(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src);

    // OPTIONAL
    void (*bd_dump)(block_device_t *bd, void (*pf)(const char *fmt, ...));

    void (*delete_block_device)(block_device_t *bd);
} block_device_impl_t;

/**
 * A block device is just a place where data can be read from and written to.
 *
 * Data is stored in blocks or sectors.
 */
struct _block_device_t {
    const block_device_impl_t * const impl;
};

/**
 * Retrieve the number of sectors available in the block device.
 *
 * THIS SHOULD VALUE SHOULD NEVER CHANGE!
 */
static inline size_t bd_num_sectors(block_device_t *bd) {
    return bd->impl->bd_num_sectors(bd);
}

/**
 * Retrieve the size of each sector.
 *
 * THIS SHOULD VALUE SHOULD NEVER CHANGE!
 */
static inline size_t bd_sector_size(block_device_t *bd) {
    return bd->impl->bd_sector_size(bd);
}

/**
 * Blocking read of sectors [sector_ind ... sector_ind + num_sectors - 1] into dest.
 *
 * dest must have size at least num_sectors * sector_size.
 *
 * Should return an error if dest is NULL, or the the sectors requested extend past the end
 * of the block device.
 *
 * NOTE: Reading from sector 0 is optionally valid. If you're block device returns an error
 * when attempting to read sector 0, this is OK!
 */
static inline fernos_error_t bd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest) {
    return bd->impl->bd_read(bd, sector_ind, num_sectors, dest);
}

/**
 * Blocking write to sectors [sector_ind ... sector_ind + num_sectors - 1] from src.
 *
 * src must have size at least num_sectors * sector_size.
 *
 * Should return an error if src is NULL, or the the sectors requested extend past the end
 * of the block device.
 *
 * NOTE: Writing to sector 0 is optionally valid. If you're block device returns an error
 * when attempting to read sector 0, this is OK!
 */
static inline fernos_error_t bd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    return bd->impl->bd_write(bd, sector_ind, num_sectors, src);
}

/**
 * If implemented, this is intended to print out some debug information about the block device.
 */
static inline void bd_dump(block_device_t *bd, void (*pf)(const char *fmt, ...)) {
    if (bd->impl->bd_dump) {
        bd->impl->bd_dump(bd, pf);
    }
}

/**
 * Delete the block device.
 */
static inline void delete_block_device(block_device_t *bd) {
    bd->impl->delete_block_device(bd);
}
