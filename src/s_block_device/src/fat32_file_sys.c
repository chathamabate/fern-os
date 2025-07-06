
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
    PROP_NULL(!al || !bd);
    PROP_NULL(bd_sector_size(bd) != 512);

    // We need to read the boot sector first.

    PROP_NULL(offset_sector >= bd_num_sectors(bd));

    // Ok now can we try to see if the given BD is even formatted as FAT32?
    // There is only so much testing we can realistically do here.
    // We'll just do some obvious checks and call it a day.

    // First verify boot sector.
    fat32_fs_boot_sector_t boot_sector;

    PROP_NULL(bd_read(bd, offset_sector, 1, &boot_sector) != FOS_SUCCESS);
    PROP_NULL(boot_sector.fat32_ebpb.super.super.bytes_per_sector != 512);

    const uint8_t sectors_per_cluster = boot_sector.fat32_ebpb.super.super.sectors_per_cluster;

    bool valid_spc = false;
    for (uint32_t i = 0; i < 8; i++) {
        if (sectors_per_cluster == (1UL << i)) {
            valid_spc = true;
            break;
        }
    }

    PROP_NULL(!valid_spc);

    const uint16_t reserved_sectors = boot_sector.fat32_ebpb.super.super.reserved_sectors;
    PROP_NULL(reserved_sectors < 2); // Need space at least for VBR and info sector.

    const uint8_t num_fats = boot_sector.fat32_ebpb.super.super.num_fats;
    PROP_NULL(num_fats == 0);

    // This should confirm FAT32.
    PROP_NULL(boot_sector.fat32_ebpb.super.super.sectors_per_fat != 0);

    // I think the point is to choose whichever field is non-zero.
    const uint32_t num_sectors = boot_sector.fat32_ebpb.super.super.num_sectors == 0 
        ? boot_sector.fat32_ebpb.super.num_sectors 
        : boot_sector.fat32_ebpb.super.super.num_sectors;

    PROP_NULL(num_sectors == 0 || num_sectors + offset_sector < offset_sector || 
            num_sectors + offset_sector > bd_num_sectors(bd));

    const uint32_t sectors_per_fat = boot_sector.fat32_ebpb.sectors_per_fat;
    PROP_NULL(sectors_per_fat == 0);

    const uint32_t root_dir_cluster = boot_sector.fat32_ebpb.root_dir_cluster;
    PROP_NULL(root_dir_cluster < 2);

    const uint32_t fs_info_sector = boot_sector.fat32_ebpb.fs_info_sector;
    PROP_NULL(fs_info_sector == 0 || fs_info_sector > num_sectors);

    PROP_NULL(boot_sector.fat32_ebpb.extended_boot_signature != 0x29);
    PROP_NULL(boot_sector.boot_signature[0] != 0x55 || boot_sector.boot_signature[1] != 0xAA);

    // Next verify info sector. (Should be less involved tbh)
    fat32_fs_info_sector_t info_sector;

    PROP_NULL(bd_read(bd, offset_sector + fs_info_sector, 1, &info_sector) != FOS_SUCCESS);

    const uint32_t sig0 = *(uint32_t *)&(info_sector.signature0);
    PROP_NULL(sig0 != 0x41615252);

    const uint32_t sig1 = *(uint32_t *)&(info_sector.signature1);
    PROP_NULL(sig1 != 0x61417272);

    const uint32_t sig2 = *(uint32_t *)&(info_sector.signature2);
    PROP_NULL(sig2 == 0xAA55);

    // Validation is done, now I think we can actually start puutting together the structure.
    
    fat32_file_sys_t *fat32_fs = al_malloc(al, sizeof(fat32_file_sys_t));
    map_t *hndl_map = new_chained_hash_map(al, sizeof(uint32_t), sizeof(fat32_file_sys_handle_t *), 
            3, hndl_map_key_eq_ft, hndl_key_hash_ft);

    if (!fat32_fs || !hndl_map) {
        delete_file_system((file_sys_t *)fat32_fs);
        delete_map(hndl_map);

        return NULL;
    }

    *(const file_sys_impl_t **)&(fat32_fs->super.impl) = &FAT32_FS_IMPL;

    // We have a lot of const fields here, unsure if this is really necessary,
    // but whatevs...

    *(allocator_t **)&(fat32_fs->al) = al;
    *(block_device_t **)&(fat32_fs->bd) = bd;
    *(uint32_t *)&(fat32_fs->bd_offset) = offset_sector;

    *(uint32_t *)&(fat32_fs->num_sectors) = num_sectors;

    *(uint32_t *)&(fat32_fs->fat_offset) = reserved_sectors;
    *(uint8_t *)&(fat32_fs->num_fats) = num_fats;
    *(uint32_t *)&(fat32_fs->sectors_per_fat) = sectors_per_fat; 

    *(uint32_t *)&(fat32_fs->data_section_offset) = reserved_sectors 
        + (num_fats * sectors_per_fat);
    *(uint8_t *)&(fat32_fs->sectors_per_cluster) = sectors_per_cluster;
    
    // Ok, now we need to calc how many data clusters are actually in the image.
    uint32_t max_clusters = (num_sectors - fat32_fs->data_section_offset) 
        / sectors_per_cluster;

    // This is the max number of clusters one fat could address.
    // Remember, 2 entries of the FAT are never used.
    const uint32_t fat_spanned_clusters = ((sectors_per_fat * 512) / sizeof(uint32_t)) - 2;
    if (fat_spanned_clusters < max_clusters) {
        max_clusters = fat_spanned_clusters;
    }

    *(uint32_t *)&(fat32_fs->num_clusters) = max_clusters;
    *(uint32_t *)&(fat32_fs->root_dir_cluster) = root_dir_cluster;

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

