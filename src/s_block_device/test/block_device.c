
#include "s_block_device/test/block_device.h"

#include <stdbool.h>
#include "k_bios_term/term.h"
#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static block_device_t *(*gen_block_device)(void) = NULL;
static block_device_t *bd = NULL;
static size_t num_sectors = 0;
static size_t sector_size = 0;
static size_t num_al_blocks = 0;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    bd = gen_block_device(); 
    TEST_TRUE(bd != NULL);

    num_sectors = bd_num_sectors(bd);
    TEST_TRUE(num_sectors >= TEST_BLOCK_DEVICE_MIN_SECTORS);

    sector_size = bd_sector_size(bd);
    TEST_TRUE(sector_size > 0);

    TEST_SUCCEED();
}

static bool posttest(void) {
    delete_block_device(bd);

    bd = NULL;
    num_sectors = 0;
    sector_size = 0;

    size_t nalb = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(num_al_blocks, nalb);

    num_al_blocks = 0;

    TEST_SUCCEED();
}

static bool test_simple_rw(void) {
    fernos_error_t err;

    uint8_t *wbuf = da_malloc(sector_size);
    uint8_t *rbuf = da_malloc(sector_size);

    for (uint32_t i = 0; i < sector_size; i++) {
        wbuf[i] = (uint8_t)i;
    }

    err = bd_write(bd, 1, 1, wbuf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = bd_read(bd, 1, 1, rbuf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t i = 0; i < sector_size; i++) {
        TEST_EQUAL_UINT((uint8_t)i, rbuf[i]);
    }

    da_free(wbuf);
    da_free(rbuf);

    TEST_SUCCEED();
}

bool test_block_device(const char *name, block_device_t *(*gen)(void)) {
    gen_block_device = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_simple_rw);
    return END_SUITE();
}
