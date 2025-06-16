
#include "s_block_device/test/mem_block_device.h"

#include "s_block_device/test/block_device.h"
#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"

static block_device_t *gen_mem_bd(void) {
    return new_da_mem_block_device(512, 512);
}

bool test_mem_block_device(void) {
    return test_block_device("Mem Block Device", gen_mem_bd);
}
