
#include "k_startup/ata_block_device.h"

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

block_device_t *get_ata_block_device(void) {
    return NULL;
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
    ata_block_device_t *abd = (ata_block_device_t *)bd;

}

static fernos_error_t abd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    ata_block_device_t *abd = (ata_block_device_t *)bd;

}

static void delete_ata_block_device(block_device_t *bd) {
    ata_block_device_t *abd = (ata_block_device_t *)bd;

}
