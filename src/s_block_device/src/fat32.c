
#include "s_block_device/fat32.h"
#include <stdbool.h>

uint32_t compute_sectors_per_fat(uint32_t total_sectors, uint16_t bytes_per_sector, 
        uint16_t reserved_sectors, uint8_t fat_copies,  uint8_t sectors_per_cluster) {
    // NOTE: write now this calculation is extremely simple. It forces only full FAT sectors to be 
    // allowed. However, this constant should not be relied on.
    // We should be able to handle the case where a FAT can occupy half a sector.
    
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

    const size_t bps = bd_sector_size(bd);
    if (bps != 512) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(bd);
    if (offset >= ns || offset + num_sectors > ns) {
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

    // Ok, our, args are valid we think. Let's write out the boot sector and 




    // Should we fix the cluster size???
    // I mean, what if the user wants a greater cluster size??
    // 
    // Oof I think this requires some math.... :(
    // Sectors for Fat and Data = (Fat Sectors * num Fats) + (Fat Sectors * Cluster Size)

    // For calculation, I think
    return FOS_SUCCESS;
}
