
#include "s_block_device/test/cached_block_device.h"
#include "s_block_device/test/block_device.h"

#include "s_block_device/block_device.h"
#include "s_block_device/cached_block_device.h"
#include "s_block_device/mem_block_device.h"
#include "k_bios_term/term.h"

static block_device_t *underlying_bd = NULL;

/**
 * Assumes underlying BD is initialized.
 */
static block_device_t *gen_cached_bd(void) {
    return new_da_cached_block_device(underlying_bd, 8, 1237);
}

bool test_cached_block_device(void) {
    underlying_bd = new_da_mem_block_device(512, 128);
    if (!underlying_bd) {
        term_put_s("Could not init underlying BD!\n");

        return false;
    }

    bool result = test_block_device("Cached Block Device", gen_cached_bd);

    delete_block_device(underlying_bd);
    underlying_bd = NULL;

    return result;

}
