
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
    return new_da_cached_block_device(underlying_bd, 8, 0);
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
    /*
     * This is a pretty rigorous test that I which I could make a little more generic,
     * but whatevs.
     *
     * It does random reads/writes to to mem_bd and a cached_bd.
     * It confirms all operations are consistent between the two block devices.
     *
     * At a certain point it flushes the cache too and confirms an underlying block device
     * has the same contents as the control.
     */

    size_t nal = al_num_user_blocks(get_default_allocator());

    fernos_error_t err;

    const size_t sector_size = 512;
    const size_t num_sectors = 300;

    block_device_t *mbd_cntl = new_da_mem_block_device(sector_size, num_sectors);
    TEST_TRUE(mbd_cntl != NULL);

    block_device_t *mbd_under = new_da_mem_block_device(sector_size, num_sectors);
    TEST_TRUE(mbd_under != NULL);

    block_device_t *cbd = new_da_cached_block_device(mbd_under, 16, 0);
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

    rand_t r = rand(0);

    for (size_t attempt = 0; attempt < 16; attempt++) {
        // We are going to do the same thing a few times.

        // Each round basically flips two coins.
        // Based on the outcome, a random read/write/read_piece/write_piece operation is performed.
        for (size_t round = 0; round < 200; round++) {
            size_t sector_ind = next_rand_u32(&r) % num_sectors;

            if (next_rand_u1(&r)) {
                // sector operation
                
                size_t sectors = (next_rand_u8(&r) % 16) + 1;

                if (sector_ind + sectors > num_sectors) {
                    sectors = num_sectors - sector_ind;
                }

                if (next_rand_u1(&r)) {
                    // bd_write

                    mem_set(big_buf0, next_rand_u8(&r), sectors * sector_size);

                    err = bd_write(mbd_cntl, sector_ind, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_write(cbd, sector_ind, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);
                } else {
                    // bd_read
                    
                    err = bd_read(mbd_cntl, sector_ind, sectors, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_read(cbd, sector_ind, sectors, big_buf1);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    TEST_TRUE(mem_cmp(big_buf0, big_buf1, sector_size * sectors));
                }
            } else {
                // bd piece functions

                size_t offset = next_rand_u16(&r) % sector_size;
                size_t bytes = (next_rand_u8(&r) % 32) + 1;

                if (offset + bytes > sector_size) {
                    bytes = sector_size - offset;
                }

                if (next_rand_u1(&r) % 2 == 0) {
                    // bd_write_piece

                    mem_set(big_buf0, next_rand_u8(&r), bytes);

                    err = bd_write_piece(mbd_cntl, sector_ind, offset, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_write_piece(cbd, sector_ind, offset, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);
                } else {
                    // bd_read_piece
                    
                    err = bd_read_piece(mbd_cntl, sector_ind, offset, bytes, big_buf0);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    err = bd_read_piece(cbd, sector_ind, offset, bytes, big_buf1);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);

                    TEST_TRUE(mem_cmp(big_buf0, big_buf1, bytes));
                }
            }
        }

        // After all rounds, let's flush the cache.

        err = bd_flush(cbd);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bd_read(mbd_cntl, 0, num_sectors, big_buf0);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bd_read(mbd_under, 0, num_sectors, big_buf1);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(mem_cmp(big_buf0, big_buf1, num_sectors * sector_size));
    }

    delete_block_device(cbd);
    delete_block_device(mbd_under);
    delete_block_device(mbd_cntl);

    da_free(big_buf0);
    da_free(big_buf1);

    size_t nalp = al_num_user_blocks(get_default_allocator());

    TEST_EQUAL_UINT(nal, nalp);

    TEST_SUCCEED();
}

bool test_cached_block_device_side_by_side(void) {
    BEGIN_SUITE("Cached BD Side By Side");
    RUN_TEST(test_side_by_side);
    return END_SUITE();
}
