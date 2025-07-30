
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

fernos_error_t parse_new_fat32_device(allocator_t *al, block_device_t *bd, uint32_t offset, 
        uint64_t seed, bool dubd, fat32_device_t **dev_out) {
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
    *(bool *)&(dev->delete_wrapped_bd) = dubd;
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

    dev->r = rand(seed);
    dev->free_q_fill = 0;

    *dev_out = dev;
        
    return FOS_SUCCESS;
}

void delete_fat32_device(fat32_device_t *dev) {
    if (!dev) {
        return;
    }

    bool delete_wrapped_bd = dev->delete_wrapped_bd;
    block_device_t *wrapped_bd = dev->bd;

    al_free(dev->al, dev);

    if (delete_wrapped_bd) {
        delete_block_device(wrapped_bd);
    }
}

fernos_error_t fat32_get_fat_slot(fat32_device_t *dev, uint32_t slot_ind, uint32_t *out_val) {
    if (!out_val) {
        return FOS_BAD_ARGS;
    }

    if (slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    const uint32_t abs_read_sector = 
        dev->bd_offset + dev->fat_offset +  
        (slot_ind / FAT32_SLOTS_PER_FAT_SECTOR);

    const uint32_t slot_index = slot_ind % FAT32_SLOTS_PER_FAT_SECTOR;

    fernos_error_t err = bd_read_piece(dev->bd, abs_read_sector, 
            slot_index * sizeof(uint32_t), sizeof(uint32_t), out_val); 

    return err;
}

fernos_error_t fat32_set_fat_slot(fat32_device_t *dev, uint32_t slot_ind, uint32_t val) {
    if (slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    const uint32_t abs_write_sector = 
        dev->bd_offset + dev->fat_offset + 
        (slot_ind / FAT32_SLOTS_PER_FAT_SECTOR);

    const uint32_t slot_index = slot_ind % FAT32_SLOTS_PER_FAT_SECTOR;

    fernos_error_t err = bd_write_piece(dev->bd, abs_write_sector, 
            slot_index * sizeof(uint32_t), sizeof(uint32_t), &val);

    return err;
}

fernos_error_t fat32_sync_fats(fat32_device_t *dev) {
    // Don't need to do any work if there is only one FAT!
    if (dev->num_fats == 1) {
        return FOS_SUCCESS;
    }

    fernos_error_t err;

    uint8_t sector_buffer[512];

    const uint32_t abs_master_fat_offset = dev->bd_offset + dev->fat_offset;

    for (uint32_t si = 0; si < dev->sectors_per_fat; si++) {
        err = bd_read(dev->bd, abs_master_fat_offset + si, 
                1, sector_buffer);
        if (err != FOS_SUCCESS) {
            return err;
        }

        for (uint8_t fi = 1; fi < dev->num_fats; fi++) {
            const uint32_t abs_copy_fat_offset = dev->bd_offset + dev->fat_offset +
                (fi * dev->sectors_per_fat);

            err = bd_write(dev->bd, abs_copy_fat_offset + si, 
                    1, sector_buffer);
            if (err != FOS_SUCCESS) {
                return err;
            }
        }
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_free_chain(fat32_device_t *dev, uint32_t slot_ind) {
    if (slot_ind < 2 || slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    fernos_error_t err;

    uint32_t curr_ind = slot_ind;
    uint32_t next_ind;

    while (!FAT32_IS_EOC(curr_ind)) {
        // Here we know curr_ind points to a valid slot index.
        
        err = fat32_get_fat_slot(dev, curr_ind, &next_ind);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        // Error out if fat[curr_ind] doesn't hold a valid value!
        if (!FAT32_IS_EOC(next_ind) && (next_ind < 2 || next_ind >= dev->num_fat_slots)) {
            return FOS_STATE_MISMATCH;
        }

        // Only overwrite fat[curr_ind] if it holds an expected value!
        err = fat32_set_fat_slot(dev, curr_ind, 0);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        // Otherwise, success!
        curr_ind = next_ind;
    }

    return FOS_SUCCESS;
}

/**
 * FOS_SUCCESS means the free queue is non-empty now.
 * FOS_NO_SPACE means we failed at finding any free entries.
 * FOS_UNKNOWN error means there was some other error.
 */
static fernos_error_t fat32_populate_free_queue(fat32_device_t *dev) {
    if (dev->free_q_fill > 0) {
        return FOS_SUCCESS;
    }

    fernos_error_t err;

    uint32_t fat_sector[FAT32_SLOTS_PER_FAT_SECTOR];

    // We will try at most 5 times to find at least one free slot.
    for (uint32_t i = 0; dev->free_q_fill == 0 && i < 5; i++) {
        // Maybe want to move away from this randow dart thow at some point, but whatevs!
        const uint32_t fat_sector_ind = next_rand_u32(&(dev->r)) % dev->sectors_per_fat;

        err = bd_read(dev->bd, dev->fat_offset + fat_sector_ind, 1, fat_sector);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        uint32_t start = 0;
        if (fat_sector_ind == 0) {
            start = 2; // On the first sector, we skip the first two slots!
        }

        uint32_t stop = FAT32_SLOTS_PER_FAT_SECTOR;
        if (fat_sector_ind == dev->sectors_per_fat - 1) {
            // On the last sector, we see if the entire FAT sector is used.
            uint32_t rem = dev->num_fat_slots % FAT32_SLOTS_PER_FAT_SECTOR;
            if (rem > 0) {
                // A remainder of 0 means the entire last FAT sector is used.
                stop = rem;
            }
        }

        for (uint32_t i = start; i < stop; i++) {
            if (fat_sector[i] == 0) {
                dev->free_q[dev->free_q_fill++] = (fat_sector_ind * FAT32_SLOTS_PER_FAT_SECTOR) + i;
            }
        }
    }

    if (dev->free_q_fill == 0) {
        return FOS_NO_SPACE;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_pop_free_fat_slot(fat32_device_t *dev, uint32_t *slot_ind) {
    if (!slot_ind) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    if (dev->free_q_fill == 0) {
        err = fat32_populate_free_queue(dev);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    uint32_t free_ind = dev->free_q[--(dev->free_q_fill)];

    err = fat32_set_fat_slot(dev, free_ind, FAT32_EOC);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    *slot_ind = free_ind;

    return FOS_SUCCESS;
}

fernos_error_t fat32_new_chain(fat32_device_t *dev, uint32_t len, uint32_t *slot_ind) {
    fernos_error_t err;

    if (!slot_ind) {
        return FOS_BAD_ARGS;
    }

    if (len == 0) {
        *slot_ind = FAT32_EOC;

        return FOS_SUCCESS;
    }

    uint32_t head;

    err = fat32_pop_free_fat_slot(dev, &head);
    if (err != FOS_SUCCESS) {
        return err;
    }

    uint32_t tail = head;

    for (uint32_t i = 1; i < len; i++) {
        uint32_t next_free;

        err = fat32_pop_free_fat_slot(dev, &next_free);
        if (err == FOS_NO_SPACE) {
            break;
        }

        if (err != FOS_SUCCESS) {
            // If we are in here, it is gauranteed, err has value FOS_UNKNOWN_ERROR,
            // but might as well be explicit.
            return FOS_UNKNWON_ERROR;
        }

        err = fat32_set_fat_slot(dev, tail, next_free);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        tail = next_free;
    }

    // If we exited the above loop do to lack of space, there should be a valid
    // non-empty chain starting at head. Let's try to free it, then return.
    if (err == FOS_NO_SPACE) {
        err = fat32_free_chain(dev, head);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        return FOS_NO_SPACE;
    }

    // Remember, we shouldn't need to write EOC to tail, since this should already
    // be done by pop_free_fat_slot.

    *slot_ind = head;

    return FOS_SUCCESS;
}

fernos_error_t fat32_resize_chain(fat32_device_t *dev, uint32_t slot_ind, uint32_t new_len) {
    if (slot_ind < 2 || slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    if (new_len == 0) {
        return fat32_free_chain(dev, slot_ind);
    }

    fernos_error_t err;

    // new_len > 0

    uint32_t iter = slot_ind;
    uint32_t links = 1; // Number of links visited thus far.
                        // starts at 1, because we know the 1st link is valid from above.

    while (true) {

        // The situation where we have already traversed `new_len` links,
        // a shrink is needed! (This logic would NOT work if `new_len` was 0)
        if (links == new_len) {
            // First get the index of the rest of the list.
            // This segment will be entirely deleted!
            uint32_t seg_head;
            err = fat32_get_fat_slot(dev, iter, &seg_head);
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            // Our list is ALREADY the correct size!
            if (FAT32_IS_EOC(seg_head)) {
                return FOS_SUCCESS;
            }

            // Otherwise, detatch and delete!
            err = fat32_set_fat_slot(dev, iter, FAT32_EOC); 
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            err = fat32_free_chain(dev, seg_head);
            if (err != FOS_SUCCESS) {
                // This will propegate malformed chained errors when attempting to delete.
                return err;
            }

            return FOS_SUCCESS;
        }
        
        uint32_t next_iter;
        err = fat32_get_fat_slot(dev, iter, &next_iter);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        // Our iterator is the end of the chain!!!
        if (FAT32_IS_EOC(next_iter)) {
            break;
        }

        if (next_iter < 2 || next_iter >= dev->num_fat_slots) {
            // Malformed chain!
            return FOS_STATE_MISMATCH;
        }

        iter = next_iter;
        links++;
    }

    uint32_t new_seg;
    err = fat32_new_chain(dev, new_len - links, &new_seg);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Attach new segment.
    err = fat32_set_fat_slot(dev, iter, new_seg);
    if (err != FOS_SUCCESS) {
        fat32_free_chain(dev, new_seg);
        return FOS_UNKNWON_ERROR;
    }
    
    return FOS_SUCCESS;
}

fernos_error_t fat32_traverse_chain(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t slot_offset, uint32_t *slot_stop_ind) {
    if (!slot_stop_ind) {
        return FOS_BAD_ARGS;
    }

    if (slot_ind < 2 || slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    fernos_error_t err;
    
    uint32_t iter = slot_ind;

    for (uint32_t i = 0; i < slot_offset; i++) {
        uint32_t next_iter;
        err = fat32_get_fat_slot(dev, iter, &next_iter);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        // We reached the end too early!
        if (FAT32_IS_EOC(next_iter)) {
            return FOS_INVALID_INDEX;
        }

        // Malformed chain.
        if (next_iter < 2 || next_iter >= dev->num_fat_slots) {
            return FOS_STATE_MISMATCH;
        }

        iter = next_iter;
    }

    *slot_stop_ind = iter;

    return FOS_SUCCESS;
}

/**
 * Returns FOS_INVALID_RANGE if the chain is not large enough!
 * Returns FOS_STATE_MISMATCH if the chain is malformed!
 */
static fernos_error_t fat32_read_write(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t sector_offset, uint32_t num_sectors, void *buf, bool write) {

    const uint32_t sector_size = bd_sector_size(dev->bd);

    if (!buf) {
        return FOS_BAD_ARGS;
    }

    if (slot_ind < 2 || slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    if (num_sectors == 0) {
        return FOS_SUCCESS;
    }

    fernos_error_t err;

    // Advance to the correct cluster index.
    uint32_t cluster_iter;
    err = fat32_traverse_chain(dev, slot_ind, 
            sector_offset / dev->sectors_per_cluster, &cluster_iter);
    if (err != FOS_SUCCESS) {
        return err;
    }

    uint32_t sectors_processed = 0;

    while (true) {
        uint8_t sectors_to_process = (num_sectors - sectors_processed);

        if (sectors_to_process > dev->sectors_per_cluster) {
            sectors_to_process = dev->sectors_per_cluster;
        }

        uint8_t shift = 0;

        // On the first iteration only, we may start from the middle of a cluster instead of the
        // beginning.
        if (sectors_processed == 0) {
            shift = sector_offset % dev->sectors_per_cluster;
            sectors_to_process -= shift;
        }

        const uint32_t abs_sector = dev->bd_offset + dev->data_section_offset + 
            ((cluster_iter - 2) * dev->sectors_per_cluster) + shift;

        if (write) {
            err = bd_write(dev->bd, abs_sector, sectors_to_process, 
                    (const uint8_t *)buf + (sectors_processed * sector_size));
        } else {
            err = bd_read(dev->bd, abs_sector, sectors_to_process, 
                    (uint8_t *)buf + (sectors_processed * sector_size));
        }

        if (err != FOS_SUCCESS) {
            return err;
        }

        sectors_processed += sectors_to_process;

        // We'll check for exact equality as a way of confirming this function was written 
        // correctly. If we overshoot the exact number of sectors we were supposed to process,
        // we'll loop forever.
        if (sectors_processed == num_sectors) {
            return FOS_SUCCESS;
        }

        // Ok we have successfully processed all the sectors we care about in this cluster,
        // let's advance to the next!

        uint32_t next_cluster;
        err = fat32_get_fat_slot(dev, cluster_iter, &next_cluster);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (FAT32_IS_EOC(next_cluster)) {
            return FOS_INVALID_RANGE;
        }

        if (next_cluster < 2 || next_cluster >= dev->num_fat_slots) {
            return FOS_STATE_MISMATCH;
        }

        // Success!
        cluster_iter = next_cluster;
    }
}

fernos_error_t fat32_read(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t sector_offset, uint32_t num_sectors, void *dest) {
    return fat32_read_write(dev, slot_ind, sector_offset, num_sectors, dest, false);
}

fernos_error_t fat32_write(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t num_sectors, const void *src) {
    return fat32_read_write(dev, slot_ind, sector_offset, num_sectors, (void *)src, true);
}

static fernos_error_t fat32_read_write_piece(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t byte_offset, uint32_t len, void *buf, bool write) {
    const uint32_t sector_size = bd_sector_size(dev->bd);

    if (!buf) {
        return FOS_BAD_ARGS;
    }

    if (byte_offset >= sector_size || len > sector_size || byte_offset + len > sector_size) {
        return FOS_INVALID_RANGE;
    }

    if (slot_ind < 2 || slot_ind >= dev->num_fat_slots) {
        return FOS_INVALID_INDEX;
    }

    fernos_error_t err;

    const uint32_t cluster_offset = sector_offset / dev->sectors_per_cluster;
    const uint32_t internal_sector_offset = sector_offset % dev->sectors_per_cluster;

    uint32_t cluster_ind;
    err = fat32_traverse_chain(dev, slot_ind, cluster_offset, &cluster_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    const uint32_t abs_sector_offset = dev->bd_offset + dev->data_section_offset +
        ((cluster_ind - 2) * dev->sectors_per_cluster) + internal_sector_offset;

    if (write) {
        err = bd_write_piece(dev->bd, abs_sector_offset, byte_offset, len, buf);
    } else {
        err = bd_read_piece(dev->bd, abs_sector_offset, byte_offset, len, buf);
    }

    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_read_piece(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t byte_offset, uint32_t len, void *dest) {
    return fat32_read_write_piece(dev, slot_ind, sector_offset, byte_offset, len, dest, false);
}

fernos_error_t fat32_write_piece(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t byte_offset, uint32_t len, const void *src) {
    return fat32_read_write_piece(dev, slot_ind, sector_offset, byte_offset, len, (void *)src, true);
}

fernos_error_t fat32_read_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, fat32_dir_entry_t *entry) {
    if (!entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    const uint32_t sector_size = bd_sector_size(dev->bd);
    const uint32_t entries_per_sector = sector_size / sizeof(fat32_dir_entry_t);

    err = fat32_read_piece(dev, slot_ind, entry_offset / entries_per_sector, 
            (entry_offset % entries_per_sector) * sizeof(fat32_dir_entry_t), 
            sizeof(fat32_dir_entry_t), entry);

    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_write_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, const fat32_dir_entry_t *entry) {
    if (!entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    const uint32_t sector_size = bd_sector_size(dev->bd);
    const uint32_t entries_per_sector = sector_size / sizeof(fat32_dir_entry_t);

    err = fat32_write_piece(dev, slot_ind, entry_offset / entries_per_sector, 
            (entry_offset % entries_per_sector) * sizeof(fat32_dir_entry_t), 
            sizeof(fat32_dir_entry_t), entry);

    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_traverse_dir_entries(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, uint32_t *sfn_entry_offset) {
    if (!sfn_entry_offset) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    // We'll start by advancing in the directory's cluster chain to save time when when use
    // the read and write functions above. No need to start at `slot_ind` if we skip the first few
    // clusters every time we read.
    uint32_t fwd_slot_ind;

    const uint32_t sector_size = bd_sector_size(dev->bd);
    const uint32_t dir_entries_per_cluster = 
        (sector_size * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    err = fat32_traverse_chain(dev, slot_ind, entry_offset / dir_entries_per_cluster, 
            &fwd_slot_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    fat32_dir_entry_t dir_entry;

    const uint32_t skipped_entries = (fwd_slot_ind - slot_ind) * dir_entries_per_cluster;
    const uint32_t fwd_entry_offset = entry_offset % dir_entries_per_cluster;
    uint32_t offset_iter = fwd_entry_offset;

    while (true) {
        err = fat32_read_dir_entry(dev, fwd_slot_ind, offset_iter, &dir_entry);
        if (err != FOS_SUCCESS) {
            return err;
        }

        if (dir_entry.raw[0] == FAT32_DIR_ENTRY_UNUSED || 
                dir_entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR) {
            // Here we reached the end of the sequence before reaching the SFN Entry :(.
            return FOS_STATE_MISMATCH;
        }

        if (!FT32F_ATTR_IS_LFN(dir_entry.short_fn.attrs)) {
            // We found our end! Wooh!
            *sfn_entry_offset = entry_offset + skipped_entries  + (offset_iter - fwd_entry_offset);
            return FOS_SUCCESS;
        }

        // Otherwise, we just keep going!
        offset_iter++;
    }
}

fernos_error_t fat32_read_file_info(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, fat32_file_info_t *info) {
    if (!info) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    // We'll start by advancing in the directory's cluster chain to save time when when use
    // the read and write functions above. No need to start at `slot_ind` if we skip the first few
    // clusters every time we read.
    uint32_t fwd_slot_ind;
    uint32_t fwd_entry_offset;

    const uint32_t sector_size = bd_sector_size(dev->bd);
    const uint32_t dir_entries_per_cluster = 
        (sector_size * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    err = fat32_traverse_chain(dev, slot_ind, entry_offset / dir_entries_per_cluster, 
            &fwd_slot_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    fwd_entry_offset = entry_offset % dir_entries_per_cluster;
    
}
