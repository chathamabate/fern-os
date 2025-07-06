
#pragma once

#include <stdint.h>
#include "s_block_device/fat32.h"
#include "s_util/misc.h"
#include "s_block_device/block_device.h"
#include "s_block_device/file_sys.h"
#include "s_mem/allocator.h"
#include "s_data/map.h"


/*
 * Now for actual file system stuff.
 */

typedef struct _fat32_file_sys_t {
    file_sys_t super;


    allocator_t * const al;

    block_device_t * const bd;
    const fat32_info_t info;
    
    /**
     * A File's first cluster index is unique across ALL files. Thus, that is the key used
     * for this map. The values are the pointers to the allocated handles themselves.
     *
     * This implementation will never hold more than one copy of a handle. 
     */
    map_t * const handle_map;
} fat32_file_sys_t;

typedef struct _fat32_file_sys_handle_t {
    /**
     * The starting cluster of this file's parent directory.
     *
     * 0 if this handle is for the root directory.
     */
    const uint32_t parent_dir_cluster;

    /**
     * Index of this file's first entry in the parent directory.
     *
     * Remember, this is index within the directory sectors. 
     * This is different than the index passed into fs_find/rm
     *
     * Undefined Value if this handle is for the root directory.
     */
    const uint32_t first_entry_index;

    /**
     * The starting cluster of this file.
     */
    const uint32_t first_file_cluster;

    /**
     * Is this a write handle?
     */
    const bool write;

    /**
     * This denotes the number of people currently using this handle. Should always be 1 for
     * write handles. When this reaches 0, the handle will likely be destroyed.
     */
    uint32_t users;

    /**
     * Is this a directory handle?
     */
    const bool dir;

    /**
     * Length in bytes of the file. If this is a directory, this value should always be a multiple
     * of the cluster size.
     */
    uint32_t len;
} fat32_file_sys_handle_t;

/**
 * Create a FAT32 file system object.
 *
 * This assumes that a valid FAT32 parition begins at `offset_sector` within the given `bd`.
 * This DOES NOT intialize FAT32 on the block device.
 * This requires a block device with sector size 512!
 *
 * The created file system DOES NOT own the given block device.
 * When the created file system is deleted, the given block device will persist.
 *
 * Returns NULL on error.
 */
file_sys_t *new_fat32_file_sys(allocator_t *al, block_device_t *bd, uint32_t offset_sector);

static inline file_sys_t *new_da_fat32_file_sys(block_device_t *bd, uint32_t offset_sector) {
    return new_fat32_file_sys(get_default_allocator(), bd, offset_sector);
}

