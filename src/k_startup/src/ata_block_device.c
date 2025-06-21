
#include "k_startup/ata_block_device.h"
#include "k_sys/ata.h"

static size_t abd_num_sectors(block_device_t *bd);
static size_t abd_sector_size(block_device_t *bd);
static fernos_error_t abd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest);
static fernos_error_t abd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src);
static void delete_ata_block_device(block_device_t *bd);

const block_device_impl_t ABD_IMPL = {
    .bd_num_sectors = abd_num_sectors,
    .bd_sector_size = abd_sector_size,
    .bd_read = abd_read,
    .bd_write = abd_write,
    .delete_block_device = delete_ata_block_device
};

static bool abd_only_inited = false;
static bool abd_only_failure_to_init = false;
static ata_block_device_t ABD_ONLY;

block_device_t *get_ata_block_device(void) {
    if (abd_only_failure_to_init) {
        return NULL; // If we fail to init once, always return NULL.
    }

    if (!abd_only_inited) {
        fernos_error_t err;

        ata_init();

        uint32_t ns;
        err = ata_num_sectors(&ns);
        if (err != FOS_SUCCESS) {
            abd_only_failure_to_init = true;
            return NULL;
        }

        /**
         * Right now we just always assume 512 byte sectors are used.
         * We could make this driver more robust by parsing the results of the IDENTIFY
         * command!
         */
        *(const block_device_impl_t **)&(ABD_ONLY.super.impl) = &ABD_IMPL;
        *(size_t *)&(ABD_ONLY.sector_size) = ATA_SECTOR_SIZE;
        *(size_t *)&(ABD_ONLY.num_sectors) = ns;

        abd_only_inited = true;
    }

    return (block_device_t *)&ABD_ONLY;
}

static size_t abd_num_sectors(block_device_t *bd) {
    ata_block_device_t *abd = (ata_block_device_t *)bd;
    
    return abd->num_sectors;
}

static size_t abd_sector_size(block_device_t *bd) {
    ata_block_device_t *abd = (ata_block_device_t *)bd;
    
    return abd->sector_size;
}

static fernos_error_t abd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest) {
    fernos_error_t err;

    ata_block_device_t *abd = (ata_block_device_t *)bd;

    if (!dest) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= abd->num_sectors || sector_ind + num_sectors > abd->num_sectors) {
        return FOS_INVALID_RANGE;
    }

    size_t total_sectors_read = 0;
    while (total_sectors_read < num_sectors) {
        size_t sectors_to_read = num_sectors - total_sectors_read;
        if (sectors_to_read > 256) {
            sectors_to_read = 256;
        }

        err = ata_read_pio(sector_ind + total_sectors_read,
                sectors_to_read == 256 ? 0 : (uint8_t)sectors_to_read,
                (uint8_t *)dest + (total_sectors_read * abd->sector_size));

        if (err != FOS_SUCCESS) {
            return err;
        }

        total_sectors_read += sectors_to_read;
    }

    return FOS_SUCCESS;
}

static fernos_error_t abd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    fernos_error_t err;

    ata_block_device_t *abd = (ata_block_device_t *)bd;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= abd->num_sectors || sector_ind + num_sectors > abd->num_sectors) {
        return FOS_INVALID_RANGE;
    }

    size_t total_sectors_written = 0;
    while (total_sectors_written < num_sectors) {
        size_t sectors_to_write = num_sectors - total_sectors_written; 
        if (sectors_to_write > 256) {
            sectors_to_write = 256;
        }

        err = ata_write_pio(sector_ind + total_sectors_written, 
                sectors_to_write == 256 ? 0 : sectors_to_write,
                (const uint8_t *)src + (total_sectors_written * abd->sector_size));

        if (err != FOS_SUCCESS) {
            return err;
        }

        total_sectors_written += sectors_to_write;
    }

    return FOS_SUCCESS;
}

/**
 * Deletion does nothing on a singleton.
 */
static void delete_ata_block_device(block_device_t *bd) {
    (void)bd;
}
