
#include "s_block_device/fat32.h"
#include <stdbool.h>
#include "s_util/str.h"

uint8_t fat32_checksum(const char *short_fn) {
    uint8_t checksum = 0;

    // Remember 8 + 3 length (No NT)
    for (uint8_t i = 0; i < 11; i++) {
        // I think this is just a rotate and add. 
        checksum = ((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + short_fn[i];
    }

    return checksum;
}

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
    // Last check should handle wrap.
    if (offset >= ns || offset + num_sectors > ns || offset + num_sectors < offset) {
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

    root_dir[1 /*2*/] = (fat32_short_fn_dir_entry_t) {
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

    // Example Long Filename directory entry!
    /*
    ((fat32_long_fn_dir_entry_t *)root_dir)[1] = (fat32_long_fn_dir_entry_t) {
        .entry_order = 0x40 | 1,

        .long_fn_0 = { 'a', 'b', 'c', '.', 't' },

        .attrs = 0x0F,
        .type = 0,

        .short_fn_checksum = fat32_checksum(root_dir[2].short_fn),

        .long_fn_1 = { 'x', 't', 0x0, 0xFFFF, 0xFFFF, 0xFFFF },

        .reserved = {0, 0},

        .long_fn_2 = { 0xFFFF, 0xFFFF }
    };
    */

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

fernos_error_t parse_new_fat32_device(allocator_t *al, block_device_t *bd, uint32_t offset, fat32_device_t **dev_out) {
    if (!bd || !dev_out) {
        return FOS_BAD_ARGS;
    }

    // We need to read the boot sector first.
    if (bd_sector_size(bd) != 512 || offset >= bd_num_sectors(bd)) {
        return FOS_INVALID_RANGE;
    }

    // Ok now can we try to see if the given BD is even formatted as FAT32?
    // There is only so much testing we can realistically do here.
    // We'll just do some obvious checks and call it a day.

    // First verify boot sector.
    fat32_fs_boot_sector_t boot_sector;

    PROP_ERR(bd_read(bd, offset, 1, &boot_sector));

    if (boot_sector.fat32_ebpb.super.super.bytes_per_sector != 512) {
        return FOS_STATE_MISMATCH;
    }

    const uint8_t sectors_per_cluster = boot_sector.fat32_ebpb.super.super.sectors_per_cluster;

    bool valid_spc = false;
    for (uint32_t i = 0; i < 8; i++) {
        if (sectors_per_cluster == (1UL << i)) {
            valid_spc = true;
            break;
        }
    }

    if (!valid_spc) {
        return FOS_STATE_MISMATCH;
    }

    const uint16_t reserved_sectors = boot_sector.fat32_ebpb.super.super.reserved_sectors;
    if (reserved_sectors < 2) {
        return FOS_STATE_MISMATCH;
    }

    const uint8_t num_fats = boot_sector.fat32_ebpb.super.super.num_fats;
    if (num_fats == 0) {
        return FOS_STATE_MISMATCH;
    }

    // This should confirm FAT32.
    if (boot_sector.fat32_ebpb.super.super.sectors_per_fat != 0) {
        return FOS_STATE_MISMATCH;
    }

    // I think the point is to choose whichever field is non-zero.
    const uint32_t num_sectors = boot_sector.fat32_ebpb.super.super.num_sectors == 0 
        ? boot_sector.fat32_ebpb.super.num_sectors 
        : boot_sector.fat32_ebpb.super.super.num_sectors;

    if (num_sectors == 0 || num_sectors + offset < offset || 
            num_sectors + offset > bd_num_sectors(bd)) {
        return FOS_STATE_MISMATCH;
    }

    const uint32_t sectors_per_fat = boot_sector.fat32_ebpb.sectors_per_fat;
    if (sectors_per_fat == 0) {
        return FOS_STATE_MISMATCH;
    }
    const uint32_t data_section_offset = reserved_sectors + (num_fats * sectors_per_fat);

    const uint32_t root_dir_cluster = boot_sector.fat32_ebpb.root_dir_cluster;
    if (root_dir_cluster < 2) {
        return FOS_STATE_MISMATCH;
    }

    const uint32_t fs_info_sector = boot_sector.fat32_ebpb.fs_info_sector;
    if (fs_info_sector == 0 || fs_info_sector > num_sectors) {
        return FOS_STATE_MISMATCH;
    }

    if (boot_sector.fat32_ebpb.extended_boot_signature != 0x29) {
        return FOS_STATE_MISMATCH;
    }

    if (boot_sector.boot_signature[0] != 0x55 || boot_sector.boot_signature[1] != 0xAA) {
        return FOS_STATE_MISMATCH;
    }

    // Next verify info sector. (Should be less involved tbh)
    fat32_fs_info_sector_t info_sector;

    PROP_ERR(bd_read(bd, offset + fs_info_sector, 1, &info_sector) != FOS_SUCCESS);

    const uint32_t sig0 = *(uint32_t *)&(info_sector.signature0);
    if (sig0 != 0x41615252) {
        return FOS_STATE_MISMATCH;
    }

    const uint32_t sig1 = *(uint32_t *)&(info_sector.signature1);
    if (sig1 != 0x61417272) {
        return FOS_STATE_MISMATCH;
    }

    const uint32_t sig2 = *(uint32_t *)&(info_sector.signature2);
    if (sig2 == 0xAA55) {
        return FOS_STATE_MISMATCH;
    }

    // Ok, now we need to calc how many data clusters are actually in the image.
    uint32_t max_clusters = (num_sectors - data_section_offset) 
        / sectors_per_cluster;

    // This is the max number of clusters one fat could address.
    // Remember, 2 entries of the FAT are never used.
    const uint32_t fat_spanned_clusters = (sectors_per_fat * FAT32_SLOTS_PER_FAT_SECTOR) - 2;
    if (fat_spanned_clusters < max_clusters) {
        max_clusters = fat_spanned_clusters;
    }

    // Woo, our validation succeeded, time to allocate a new FAT32 device!

    fat32_device_t *dev = al_malloc(al, sizeof(fat32_device_t));

    if (!dev) {
        return FOS_NO_MEM;
    }

    *(allocator_t **)&(dev->al) = al;
    *(block_device_t **)&(dev->bd) = bd;
    *(uint32_t *)&(dev->bd_offset) = offset;
    *(uint32_t *)&(dev->num_sectors) = num_sectors;
    *(uint16_t *)&(dev->fat_offset) = reserved_sectors;
    *(uint8_t *)&(dev->num_fats) = num_fats;
    *(uint32_t *)&(dev->sectors_per_fat) = sectors_per_fat;
    *(uint32_t *)&(dev->data_section_offset) = reserved_sectors + (sectors_per_fat * num_fats);
    *(uint8_t *)&(dev->sectors_per_cluster) = sectors_per_cluster;
    *(uint32_t *)&(dev->num_fat_slots) = max_clusters + 2;
    *(uint32_t *)&(dev->root_dir_cluster) = root_dir_cluster;

    *dev_out = dev;
        
    return FOS_SUCCESS;
}

void delete_fat32_device(fat32_device_t *dev) {
    al_free(dev->al, dev);
}

fernos_error_t fat32_get_fat_slot(fat32_device_t *dev, uint8_t fat, uint32_t slot_ind, uint32_t *out_val) {
    if (!out_val) {
        return FOS_BAD_ARGS;
    }

    if (fat > dev->num_fats || slot_ind > dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    const uint32_t abs_read_sector = 
        dev->bd_offset + dev->fat_offset + (fat * dev->sectors_per_fat) + 
        (slot_ind / FAT32_SLOTS_PER_FAT_SECTOR);

    const uint32_t slot_index = slot_ind % FAT32_SLOTS_PER_FAT_SECTOR;

    fernos_error_t err = bd_read_piece(dev->bd, abs_read_sector, 
            slot_index * sizeof(uint32_t), sizeof(uint32_t), out_val); 

    return err;
}

fernos_error_t fat32_set_fat_slot(fat32_device_t *dev, uint8_t fat, uint32_t slot_ind, uint32_t val) {
    if (fat > dev->num_fats || slot_ind > dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    const uint32_t abs_write_sector = 
        dev->bd_offset + dev->fat_offset + (fat * dev->sectors_per_fat) + 
        (slot_ind / FAT32_SLOTS_PER_FAT_SECTOR);

    const uint32_t slot_index = slot_ind % FAT32_SLOTS_PER_FAT_SECTOR;

    fernos_error_t err = bd_write_piece(dev->bd, abs_write_sector, 
            slot_index * sizeof(uint32_t), sizeof(uint32_t), &val);

    return err;
}

fernos_error_t fat32_sync_fats(fat32_device_t *dev, uint8_t master_fat) {
    if (master_fat > dev->num_fats) {
        return FOS_INVALID_INDEX;
    }

    // Don't need to do any work if there is only one FAT!
    if (dev->num_fats == 1) {
        return FOS_SUCCESS;
    }

    fernos_error_t err;

    uint8_t sector_buffer[512];

    const uint32_t abs_master_fat_offset = dev->bd_offset + dev->fat_offset + 
        (master_fat * dev->sectors_per_fat);

    for (uint32_t si = 0; si < dev->sectors_per_fat; si++) {
        err = bd_read(dev->bd, abs_master_fat_offset + si, 
                1, sector_buffer);
        if (err != FOS_SUCCESS) {
            return err;
        }

        for (uint8_t fi = 0; fi < dev->num_fats; fi++) {
            if (fi != master_fat) {
                const uint32_t abs_copy_fat_offset = dev->bd_offset + dev->fat_offset +
                    (fi * dev->sectors_per_fat);

                err = bd_write(dev->bd, abs_copy_fat_offset + si, 
                        1, sector_buffer);
                if (err != FOS_SUCCESS) {
                    return err;
                }
            }
        }
    }

    return FOS_SUCCESS;
}
