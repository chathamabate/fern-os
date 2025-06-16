
#include "k_startup/test/ata_block_device.h"

#include "s_block_device/test/block_device.h"
#include "k_startup/ata_block_device.h"

bool test_ata_block_device(void) {
    return test_block_device("ATA Block Device", get_ata_block_device);
}
