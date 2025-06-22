
#include "s_block_device/fat32.h"
#include <stdbool.h>

// We really need sectors per fat and complete fats.
uint32_t compute_num_complete_fat_sectors(uint32_t total_sectors, uint16_t bytes_per_sector, 
        uint16_t reserved_sectors, uint8_t fat_copies,  uint8_t sectors_per_cluster) {
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

fernos_error_t init_fat32(block_device_t *bd, uint32_t offset, uint32_t num_sectors) {

    // Should we fix the cluster size???
    // I mean, what if the user wants a greater cluster size??
    // 
    // Oof I think this requires some math.... :(
    // Sectors for Fat and Data = (Fat Sectors * num Fats) + (Fat Sectors * Cluster Size)

    // For calculation, I think
    return FOS_SUCCESS;
}
