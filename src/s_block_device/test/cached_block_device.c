
#include "s_block_device/test/cached_block_device.h"
#include "s_block_device/test/block_device.h"

#include "s_block_device/block_device.h"
#include "s_block_device/cached_block_device.h"
#include "s_block_device/mem_block_device.h"
#include "k_bios_term/term.h"
#include "s_util/rand.h"
#include "s_util/str.h"

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

/*
 * Below is a kinda more comprehensive test specifically for the cached BD.
 * It understands that there is an "underlying BD"
 *
 * It assumes the mem_block_device works.
 */

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static bool test_side_by_side(void) {
    fernos_error_t err;

    const size_t sector_size = 512;
    const size_t num_sectors = 300;

    block_device_t *mbd_cntl = new_da_mem_block_device(sector_size, num_sectors);
    TEST_TRUE(mbd_cntl != NULL);

    block_device_t *mbd_under = new_da_mem_block_device(sector_size, num_sectors);
    TEST_TRUE(mbd_under != NULL);

    block_device_t *cbd = new_da_cached_block_device(mbd_under, 16, 123891);
    TEST_TRUE(cbd != NULL);

    uint8_t *big_buf0 = da_malloc(sector_size * num_sectors);
    TEST_TRUE(big_buf0 != NULL);

    uint8_t *big_buf1 = da_malloc(sector_size * num_sectors);
    TEST_TRUE(big_buf1 != NULL);

    // Start by clearing both block devices.

    mem_set(big_buf0, 0, sector_size * num_sectors);

    err = bd_write(mbd_cntl, 0, num_sectors, big_buf0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = bd_write(cbd, 0, num_sectors, big_buf0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    rand_t r = rand(1238489);

    for (size_t attempt = 0; attempt < 4; attempt++) {
        // We are going to do the same thing a few times.

        // Each write round we will either call bd_write or bd_write_piece.
        for (size_t round = 0; round < 100; round++) {
            uint32_t pick = next_rand(&r);
            size_t offset_sector = pick % num_sectors;

            if (next_rand(&r) % 2 == 0) {
                // sector operation
                
                size_t sectors = (pick % 8) + 1;

                if (offset_sector + sectors > num_sectors) {
                    sectors = num_sectors - offset_sector;
                }

                if (next_rand(&r) % 2 == 0) {
                    // bd_write
                    LOGF_METHOD("BD Write %u %u\n", offset_sector, sectors);

                    mem_set(big_buf0, pick % 256, sectors * sector_size);

                    err = bd_write(mbd_cntl, offset_sector, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_write(cbd, offset_sector, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);
                } else {
                    // bd_read
                    
                    //LOGF_METHOD("BD Read %u %u\n", offset_sector, sectors);

                    err = bd_read(mbd_cntl, offset_sector, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_read(cbd, offset_sector, sectors, big_buf1);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    TEST_TRUE(mem_cmp(big_buf0, big_buf1, sector_size * sectors));
                }
            } else {
                // bd piece functions

                size_t offset_index = pick % sector_size;
                size_t bytes = (pick % 32) + 1;

                if (offset_index + bytes > sector_size) {
                    bytes = sector_size - offset_index;
                }

                if (next_rand(&r) % 2 == 0) {
                    // bd_write_piece

                    //LOGF_METHOD("BD Write Piece %u %u %u\n", offset_sector, offset_index, bytes);
                    mem_set(big_buf0, pick % 256, bytes);

                    err = bd_write_piece(mbd_cntl, offset_sector, offset_index, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_write_piece(cbd, offset_sector, offset_index, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);
                } else {
                    // bd_read_piece
                    
                    LOGF_METHOD("BD Read Piece %u %u %u\n", offset_sector, offset_index, bytes);
                    err = bd_read_piece(mbd_cntl, offset_sector, offset_index, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_read_piece(cbd, offset_sector, offset_index, bytes, big_buf1);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    TEST_TRUE(mem_cmp(big_buf0, big_buf1, bytes));
                }

            }
        }
    }

    TEST_SUCCEED();
}

bool test_cached_block_device_side_by_side(void) {
    BEGIN_SUITE("Cached BD Side By Side");
    RUN_TEST(test_side_by_side);
    return END_SUITE();
}
