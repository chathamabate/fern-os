
#include "s_block_device/fat32.h"
#include <stdbool.h>

uint32_t compute_sectors_per_fat(uint32_t total_sectors, uint16_t bytes_per_sector, 
        uint16_t reserved_sectors, uint8_t fat_copies,  uint8_t sectors_per_cluster) {
    // NOTE: write now this calculation is extremely simple. It forces only full FAT sectors to be 
    // allowed. However, this constant should not be relied on.
    // We should be able to handle the case where a FAT can occupy half a sector.
    
    if (total_sectors < reserved_sectors) {
        return 0;
    }

    const uint32_t available_sectors = total_sectors - reserved_sectors;

    /**
     * How many data sectors are required to add a single complete FAT sector.
     * Assumes `bytes_per_sector` is divisible by 4.
     */
    const uint32_t data_sectors_per_fat_sector = (bytes_per_sector / 4) * sectors_per_cluster;

    // Remember each "unique fat" is mirrored `fat_copies` times.
    const uint32_t complete_fats = available_sectors / (fat_copies + data_sectors_per_fat_sector);

    return complete_fats; 
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

    const uint32_t fat_copies = 2;

    // For now we will NOT copy the boot sector/fs_info sector.
    const uint32_t spf = compute_sectors_per_fat(num_sectors, bps, 2, fat_copies, 
            sectors_per_cluster);

    if (spf == 0) {
        return FOS_BAD_ARGS; 
    }

    // Ok, our, args are valid we think. Let's write out the boot sector and fs_info sector.

    fat32_fs_boot_sector_t boot_sector = {
        .oem_name = {'F', 'e', 'r', 'n', 'O', 'S', ' ', ' '},
        .fat32_ebpb = {
            .super = {
                .super = {
                    .bytes_per_sector = 512,
                    .sectors_per_cluster = sectors_per_cluster,
                    .reserved_sectors = 2, // No boot sector copying.
                    .num_fats = fat_copies,

                    .num_small_fat_root_dir_entires = 0,
                    .num_sectors = 0,
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
            .major_version = 0,

            .root_dir_cluster = 2, // Open to changing this.

            .fs_info_sector = 1, // Right after the boot sector.
            
            .boot_sectors_copy_sector_start = 0, // no boot sector backup
            .reserved0 = {0},
            .drive_number = 0,
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

        // I may plan not to keep these updated tbh...
        .num_free_clusters = 0xFFFFFFFF,
        .last_allocated_date_cluster = 0xFFFFFFFF ,

        .reserved1 = {0},

        .signature2 = {
            0x0, 0x0, 0x55, 0xAA
        }
    };

    err = bd_write(bd, offset + 1, 1, &info_sector);
    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}
