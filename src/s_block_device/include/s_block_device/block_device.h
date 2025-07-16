
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

    fernos_error_t (*bd_read_piece)(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest);
    fernos_error_t (*bd_write_piece)(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src);

    // OPTIONAL
    fernos_error_t (*bd_flush)(block_device_t *bd);

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
 */
static inline fernos_error_t bd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    return bd->impl->bd_write(bd, sector_ind, num_sectors, src);
}

/*
 * NOTE: bd_read_piece and bd_write_piece have been added to this interface to allow for 
 * the possibility of efficient reads/writes of a single sector. For example, a BD implementation
 * may involve so form of caching. In which case, reading/writing in units of full sectors
 * may not be necessary.
 *
 * The implemeter doesn't need to make their implementations of these functions efficient though.
 * They can always just be wrappers of bd_write and bd_read above.
 */

/**
 * Blocking read of a piece of data within a single sector.
 *
 * dest must have size at least len.
 *
 * Should return an error if dest is NULL, or if the piece requested extends past the sector
 * boundary.
 */
static inline fernos_error_t bd_read_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest) {
    return bd->impl->bd_read_piece(bd, sector_ind, offset, len, dest);
}

/**
 * Blocking write of a piece of data within a single sector.
 *
 * src must have size at least len.
 *
 * Should return an error if src is NULL, or if the piece requested extends past the sector
 * boundary.
 */
static inline fernos_error_t bd_write_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src) {
    return bd->impl->bd_write_piece(bd, sector_ind, offset, len, src);
}

/**
 * If implemented, this function should perform some sort of implementation specific cache
 * flush.
 *
 * All read/write functions should be opaque to cache flushes. What I mean here is that writes
 * should always be immediately visible to subsequent reads without the need for a flush.
 *
 * What the flush does should never affect the outputs of any of the functions in this interface.
 * It'll likely be something you want to do before deleting a block device.
 */
static inline fernos_error_t bd_flush(block_device_t *bd) {
    if (bd->impl->bd_flush) {
        return bd->impl->bd_flush(bd);
    }

    return FOS_SUCCESS;
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
