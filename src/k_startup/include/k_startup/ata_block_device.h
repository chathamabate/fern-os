
#pragma once

#include "s_block_device/block_device.h"
#include <stdint.h>

typedef struct _ata_block_device_t {
    block_device_t super;

    const size_t num_sectors;
    const size_t sector_size;

    /**
     * This is used as a buffer for the read/write piece functions.
     *
     * The ATA block device does not provide an efficient read/write piece 
     * implementation. 
     */
    uint8_t piece_buffer[512];
} ata_block_device_t;

/**
 * The ATA block device will be a singleton which uses the ATA primary drive.
 *
 * Use this function to get the ATA Block Device.
 */
block_device_t *get_ata_block_device(void);
