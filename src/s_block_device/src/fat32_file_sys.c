
#include "s_block_device/fat32_file_sys.h"
#include <stdbool.h>
#include "s_util/str.h"
#include "k_bios_term/term.h"
#include "s_util/misc.h"

uint32_t compute_sectors_per_fat(uint32_t total_sectors, uint16_t bytes_per_sector, 
        uint16_t reserved_sectors, uint8_t fat_copies,  uint8_t sectors_per_cluster) {
    if (total_sectors < reserved_sectors) {
        return 0;
    }

    const uint32_t available_sectors = total_sectors - reserved_sectors;

    /**
     * How many data sectors are required to add a single complete FAT sector.
     * Assumes `bytes_per_sector` is divisible by 4.
     */
    const uint32_t data_sectors_per_fat_sector = (bytes_per_sector / 4) * sectors_per_cluster;

    /**
     * NOTE: I have learned that if that FAT is not large enough to cover the ENTIRE area,
     * FAT validation will FAIL.
     */

    const uint32_t complete_fat_sectors = fat_copies + data_sectors_per_fat_sector;

    if (available_sectors % complete_fat_sectors == 0) {
        return available_sectors / complete_fat_sectors;
    } 

    return (available_sectors / complete_fat_sectors) + 1;
}

fernos_error_t init_fat32(block_device_t *bd, uint32_t offset, uint32_t num_sectors, 
        uint32_t sectors_per_cluster) {
    fernos_error_t err;

    const size_t bps = bd_sector_size(bd);
    if (bps != 512) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(bd);
    if (offset >= ns || offset + num_sectors > ns || ns > num_sectors) {
        return FOS_INVALID_RANGE;
    }

    // Valid options: 1, 2, 4, 8, 16, 32, 64, 128
    bool valid = false;
    for (uint32_t shift = 0; shift < 8; shift++) {
        if (sectors_per_cluster == 1UL << shift) {
            valid = true;
            break;
        }
    }

    if (!valid) {
        return FOS_BAD_ARGS;
    }

    const uint32_t reserved_sectors = 32;
    const uint32_t fat_copies = 2;

    // For now we will NOT copy the boot sector/fs_info sector.
    const uint32_t spf = compute_sectors_per_fat(num_sectors, bps, 
            reserved_sectors, fat_copies, sectors_per_cluster);

    if (spf == 0) {
        return FOS_BAD_ARGS; 
    }

    const uint32_t used_sectors = reserved_sectors + (spf * fat_copies);
    if (num_sectors < used_sectors) {
        return FOS_INVALID_RANGE;
    }
    
    // Make sure we have at LEAST 16 data clusters.
    const uint32_t available_data_clusters = (num_sectors - used_sectors) / sectors_per_cluster;
    if (available_data_clusters < 16) {
        return FOS_INVALID_RANGE;
    }

    // Ok, our, args are valid we think. Let's write out the boot sector and fs_info sector.

    fat32_fs_boot_sector_t boot_sector = {
        .oem_name = {'F', 'e', 'r', 'n', 'O', 'S', ' ', ' '},
        .fat32_ebpb = {
            .super = {
                .super = {
                    .bytes_per_sector = 512,
                    .sectors_per_cluster = sectors_per_cluster,
                    .reserved_sectors = reserved_sectors, // No boot sector copying.
                    .num_fats = fat_copies,

                    .num_small_fat_root_dir_entires = 0,

                    .num_sectors = num_sectors,
                    .media_descriptor = 0xF8,
                    .sectors_per_fat = 0
                },
                
                // 1s for LBA.
                .sectors_per_track = 1,
                .heads_per_disk = 1,

                .volume_absolute_offset = offset,
                .num_sectors = num_sectors,
            },

            .sectors_per_fat = spf,
            .ext_flags = 0, // mirroring will be enabled.

            .minor_version = 0,
            .major_version = 1,

            /*
             * NOTE: IT is EXTREMELY IMPORTANT to understand that data cluster 2 is actually
             * the first data cluster right after the FAT!
             */

            .root_dir_cluster = 2, // Open to changing this.

            .fs_info_sector = 1, // Right after the boot sector.
            
            .boot_sectors_copy_sector_start = 0, // no boot sector backup
            .reserved0 = {0},
            .drive_number = 0x80,
            .reserved1 = 0,
            .extended_boot_signature = 0x29,
            .serial_number = 0,

            .volume_label = {
                'F', 'e', 'r', 'n', 'V', 'o', 'l', 
                ' ', ' ' , ' ', ' ' 
            },

            .fs_label = {
                'F', 'A', 'T', '3', '2', '.', 'f', 's'
            }
        },

        .boot_code = {0},

        .boot_signature = {
            0x55, 0xAA
        }
    };

    err = bd_write(bd, offset, 1, &boot_sector);
    if (err != FOS_SUCCESS) {
        return err;
    }

    fat32_fs_info_sector_t info_sector = {
        .signature0 = {
            0x52, 0x52, 0x61, 0x41
        },

        .reserved0 = {0},

        .signature1 = {
            0x72, 0x72, 0x41, 0x61
        },

        // Subtract 3 for first two reserved, root dir, and readme
        .num_free_clusters = 0xFFFFFFFF,
        .last_allocated_date_cluster = 0xFFFFFFFF,

        .reserved1 = {0},

        .signature2 = {
            0x0, 0x0, 0x55, 0xAA
        }
    };

    err = bd_write(bd, offset + 1, 1, &info_sector);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // So the FATs are going to start right after the boot and fs info sectors.

    uint32_t fat_sector[512 / sizeof(uint32_t)] = {0};

    // First let's write the first FAT sector.
    fat_sector[0] = 0x0FFFFFF8;
    fat_sector[1] = 0xFFFFFFFF;

    fat_sector[2] = FAT32_EOC; // Root directory.
    fat_sector[3] = FAT32_EOC; // README.TXT

    for (uint32_t f = 0; f < fat_copies; f++) {
        err = bd_write(bd, offset + reserved_sectors + (f * spf), 1, &fat_sector);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    // 0 out the rest of the FAT sectors.
    mem_set(fat_sector,0, 512);
    for (uint32_t f = 0; f < fat_copies; f++) {
        for (uint32_t s = 1; s < spf; s++) {
            err = bd_write(bd, offset + reserved_sectors + (f * spf) + s, 1, &fat_sector);
            if (err != FOS_SUCCESS) {
                return err;
            }
        }
    }

    // Write out root directory and readme contents!

    const uint32_t data_section_offset = offset + reserved_sectors + (fat_copies * spf);
    
    fat32_short_fn_dir_entry_t root_dir[512 / sizeof(fat32_short_fn_dir_entry_t)] = {0};

    // Pointer back to self.
    root_dir[0] = (fat32_short_fn_dir_entry_t) {
        .short_fn = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .extenstion = {' ', ' ', ' '},
        .attrs = FT32F_ATTR_SUBDIR,
        .reserved = 0,

        // Creation stuff not speicifed.

        .first_cluster_high = 0,
        
        .last_write_time = fat32_time(0, 0, 0),
        .last_access_date = fat32_date(1, 1, 0),

        .first_cluster_low = 2,
        .files_size = 0
    };

    char readme_txt[512] = "First FernOS File!";
    size_t readme_txt_size = str_len(readme_txt) + 1;

    root_dir[1] = (fat32_short_fn_dir_entry_t) {
        .short_fn = {'R', 'E', 'A', 'D', 'M', 'E', ' ', ' '},
        .extenstion = {'T', 'X', 'T'},
        .attrs = 0,
        .reserved = 0,

        // Creation stuff not speicifed.

        .first_cluster_high = 0,
        
        .last_write_time = fat32_time(0, 0, 0),
        .last_access_date = fat32_date(1, 0, 0),

        .first_cluster_low = 3,
        .files_size = readme_txt_size
    };

    // Remember, the root directory as a whole is more than just one sector!

    // Write out root directory.
    err = bd_write(bd, data_section_offset, 1, root_dir);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Make sure ALL of the root directory is zeroed out!
    mem_set(root_dir, 0, sizeof(root_dir));
    for (uint32_t s = 1; s < sectors_per_cluster; s++) {
        err = bd_write(bd, data_section_offset + s, 1, root_dir);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    // Write out README.TXT
    err = bd_write(bd, data_section_offset + (1 * sectors_per_cluster), 1, readme_txt);
    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

/*
 * FAT32 FS Work.
 */

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

    PROP_NULL(offset_sector < bd_num_sectors(bd));

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

    const uint8_t num_fats = boot_sector.fat32_ebpb.super.super.num_fats;
    PROP_NULL(num_fats == 0);

    const uint32_t num_sectors = boot_sector.fat32_ebpb.super.super.num_sectors;
    PROP_NULL(num_sectors == 0 || num_sectors + offset_sector < offset_sector || 
            num_sectors + offset_sector > bd_num_sectors(bd));

    // This should confirm FAT32.
    PROP_NULL(boot_sector.fat32_ebpb.super.super.sectors_per_fat != 0);

    const uint32_t sectors_per_fat = boot_sector.fat32_ebpb.sectors_per_fat;
    PROP_NULL(sectors_per_fat == 0);

    const uint32_t root_dir_cluster = boot_sector.fat32_ebpb.root_dir_cluster;
    PROP_NULL(root_dir_cluster < 2);

    const uint32_t fs_info_sector = boot_sector.fat32_ebpb.fs_info_sector;
    PROP_NULL(fs_info_sector == 0 || fs_info_sector > num_sectors);

    PROP_NULL(boot_sector.fat32_ebpb.extended_boot_signature != 0x29);
    PROP_NULL(boot_sector.boot_signature[0] != 0x55 || boot_sector.boot_signature[1] != 0xA);

    // Next verify info sector.
    fat32_fs_info_sector_t info_sector;

    PROP_NULL(bd_read(bd, offset_sector + 1, 1, &info_sector) != FOS_SUCCESS);

    return NULL;
}

static void delete_fat32_file_system(file_sys_t *fs) {
}

static void fat32_fs_return_handle(file_sys_t *fs, file_sys_handle_t hndl) {
}

static fernos_error_t fat32_fs_find_root_dir(file_sys_t *fs, bool write, 
        file_sys_handle_t *out) {
    return FOS_NOT_IMPLEMENTED;
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

