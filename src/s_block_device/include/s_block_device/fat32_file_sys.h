
#pragma once

#include "s_block_device/fat32.h"
#include "s_block_device/fat32_dir.h"
#include "s_block_device/file_sys.h"
#include "s_mem/allocator.h"
#include "s_util/datetime.h"

typedef struct _fat32_file_sys_t {
    file_sys_t super;

    allocator_t * const al;

    fat32_device_t * const dev;

    const dt_producer_ft now;
} fat32_file_sys_t;

typedef struct _fat32_fs_node_key_val_t {
    /**
     * Whether or not the node pointed to by this key is a directory.
     */
    const bool is_dir;

    /**
     * Index into the FAT of the parent directories starting cluster.
     *
     * ONLY USED if `is_dir` is FALSE.
     */
    const uint32_t parent_slot_ind;

    /**
     * Index into the parent directory of this node's SFN entry.
     *
     * ONLY USED if `is_dir` is FALSE.
     */
    const uint32_t sfn_entry_offset;

    /**
     * The index into the FAT of the first cluster of this node.
     */
    const uint32_t starting_slot_ind;
} fat32_fs_node_key_val_t;

/*
 * NOTE: This implementation is pretty simple.
 *
 * Most notably, to avoid some annoying back tracking code, directory timestamps ARE NOT USED.
 * Created directories will be given timestamps of 0.
 *
 * Only actual data files will have their timestamp information updated.
 */

typedef const fat32_fs_node_key_val_t *fat32_fs_node_key_t;

/**
 * Create a new fat32 file system object by parsing a FAT32 formatted block device.
 * `offset` should be the very beginning of the parition in `bd`. (unit of sectors)
 *
 * `dubd` stands for "delete underlying block device". If true, when this file system is deleted,
 * so will the given block device. Otherwise, the given block device will persist past the lifetime
 * of the file system object.
 *
 * On success, FOS_SUCCESS is returned, the created file system is written to `*fs_out`.
 * On failure, the given block device will NEVER be deleteed.
 */
fernos_error_t parse_new_fat32_file_sys(allocator_t *al, block_device_t *bd, uint32_t offset,
        uint64_t seed, bool dubd, dt_producer_ft now, file_sys_t **fs_out);

static inline fernos_error_t parse_new_da_fat32_file_sys(block_device_t *bd, uint32_t offset,
        uint64_t seed, bool dubd, dt_producer_ft now, file_sys_t **fs_out) {
    return parse_new_fat32_file_sys(get_default_allocator(), bd, offset, seed, dubd, now, fs_out);
}
