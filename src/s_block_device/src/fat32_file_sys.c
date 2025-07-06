
#include "s_block_device/fat32_file_sys.h"
#include <stdbool.h>
#include "s_util/str.h"
#include "k_bios_term/term.h"
#include "s_util/misc.h"


/*
 * FAT32 FS Work.
 */

static bool hndl_map_key_eq_ft(chained_hash_map_t *chm, const void *k0, const void *k1) {
    (void)chm;

    return *(uint32_t *)k0 == *(uint32_t *)k1;
}

static uint32_t hndl_key_hash_ft(chained_hash_map_t *chm, const void *k) {
    (void)chm;

    return ((((*(uint32_t *)k) + 133) * 7) + 1) * 3; // kinda random hash func.
}

static void delete_fat32_file_system(file_sys_t *fs);
static void fat32_fs_return_handle(file_sys_t *fs, file_sys_handle_t hndl);
static fernos_error_t fat32_fs_find_root_dir(file_sys_t *fs, bool write, file_sys_handle_t *out);
static fernos_error_t fat32_fs_get_info(file_sys_t *fs, file_sys_handle_t hndl, file_sys_info_t *out);
static fernos_error_t fat32_fs_is_write_handle(file_sys_t *fs, file_sys_handle_t hndl, bool *out);
static fernos_error_t fat32_fs_find(file_sys_t *fs, file_sys_handle_t dir_hndl, size_t index, bool write, file_sys_handle_t *out);
static fernos_error_t fat32_fs_get_parent_dir(file_sys_t *fs, file_sys_handle_t hndl, bool write, file_sys_handle_t *out);
static fernos_error_t fat32_fs_mkdir(file_sys_t *fs, file_sys_handle_t dir, const char *fn, file_sys_handle_t *out);
static fernos_error_t fat32_fs_touch(file_sys_t *fs, file_sys_handle_t dir, const char *fn, file_sys_handle_t *out);
static fernos_error_t fat32_fs_rm(file_sys_t *fs, file_sys_handle_t dir, size_t index);
static fernos_error_t fat32_fs_read(file_sys_t *fs, file_sys_handle_t hndl, size_t offset, size_t len, void *dest, size_t *readden);
static fernos_error_t fat32_fs_write(file_sys_t *fs, file_sys_handle_t hndl, size_t offset, size_t len, const void *src, size_t *written);

static const file_sys_impl_t FAT32_FS_IMPL = {
    .delete_file_system = delete_fat32_file_system,
    .fs_return_handle = fat32_fs_return_handle,
    .fs_find_root_dir = fat32_fs_find_root_dir,
    .fs_get_info = fat32_fs_get_info,
    .fs_is_write_handle = fat32_fs_is_write_handle,
    .fs_find = fat32_fs_find,
    .fs_get_parent_dir = fat32_fs_get_parent_dir,
    .fs_mkdir = fat32_fs_mkdir,
    .fs_touch = fat32_fs_touch,
    .fs_rm = fat32_fs_rm,
    .fs_read = fat32_fs_read,
    .fs_write = fat32_fs_write,
};

file_sys_t *new_fat32_file_sys(allocator_t *al, block_device_t *bd, uint32_t offset_sector) {
    fernos_error_t err;

    PROP_NULL(!al || !bd);

    fat32_file_sys_t *fat32_fs = al_malloc(al, sizeof(fat32_file_sys_t));

    if (fat32_fs) {
        // Parse the block device for FAT32.
        err = parse_fat32(bd, offset_sector, (fat32_info_t *)&(fat32_fs->info));
        if (err != FOS_SUCCESS) {
            al_free(al, fat32_fs);
            return NULL;
        }
    } else {
        return NULL;
    }

    map_t *hndl_map = new_chained_hash_map(al, sizeof(uint32_t), sizeof(fat32_file_sys_handle_t *), 
            3, hndl_map_key_eq_ft, hndl_key_hash_ft);

    if (!hndl_map) {
        al_free(al, fat32_fs);
        return NULL;
    }

    *(const file_sys_impl_t **)&(fat32_fs->super.impl) = &FAT32_FS_IMPL;

    *(allocator_t **)&(fat32_fs->al) = al;
    *(block_device_t **)&(fat32_fs->bd) = bd;

    *(map_t **)&(fat32_fs->handle_map) = hndl_map;

    return (file_sys_t *)fat32_fs;
}

static void delete_fat32_file_system(file_sys_t *fs) {
    // TODO
}

/**
 * Get the number of files within a directory.
 */
static fernos_error_t fat32_num_used_dir_entries(fat32_file_sys_t *fat32_fs, uint32_t dir_cluster, 
        uint32_t *out) {
    /*
    uint32_t sub_files = 0;    

    const uint32_t num_entries = 512 / sizeof(fat32_short_fn_dir_entry_t);

    uint8_t sector[512];

    fat32_short_fn_dir_entry_t *sfn_entries = (fat32_short_fn_dir_entry_t *)sector;
    fat32_long_fn_dir_entry_t *lfn_entries = (fat32_long_fn_dir_entry_t *)sector;
    */

    return FOS_NOT_IMPLEMENTED;
    
}

/* FS Interface implementations */

static void fat32_fs_return_handle(file_sys_t *fs, file_sys_handle_t hndl) {
    // TODO
}

static fernos_error_t fat32_fs_find_root_dir(file_sys_t *fs, bool write, 
        file_sys_handle_t *out) {
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;

    if (!fs || !out) {
        return FOS_BAD_ARGS;
    }

    uint32_t rd_cluster = fat32_fs->root_dir_cluster;
    fat32_file_sys_handle_t **hndl_ptr = mp_get(fat32_fs->handle_map, &rd_cluster);

    fat32_file_sys_handle_t *hndl;

    if (hndl_ptr) {
        hndl = *hndl_ptr;
    } else {
        // Damn, I think imma need some fat32 helpers tbh....
        // Maybe defined above this as like driver stuff???
        // No root directory handle, time to create a new one!
    }

    return FOS_SUCCESS;
}

static fernos_error_t fat32_fs_get_info(file_sys_t *fs, file_sys_handle_t hndl, 
        file_sys_info_t *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_is_write_handle(file_sys_t *fs, file_sys_handle_t hndl, bool *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_find(file_sys_t *fs, file_sys_handle_t dir_hndl, size_t index, 
        bool write, file_sys_handle_t *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_get_parent_dir(file_sys_t *fs, file_sys_handle_t hndl, 
        bool write, file_sys_handle_t *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_mkdir(file_sys_t *fs, file_sys_handle_t dir, const char *fn, file_sys_handle_t *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_touch(file_sys_t *fs, file_sys_handle_t dir, const char *fn, 
        file_sys_handle_t *out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_rm(file_sys_t *fs, file_sys_handle_t dir, size_t index) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_read(file_sys_t *fs, file_sys_handle_t hndl, size_t offset, 
        size_t len, void *dest, size_t *readden) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_write(file_sys_t *fs, file_sys_handle_t hndl, size_t offset, 
        size_t len, const void *src, size_t *written) {
    return FOS_NOT_IMPLEMENTED;
}

