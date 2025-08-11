
#include "s_block_device/test/fat32.h"

#include <stdbool.h>

#include "s_util/rand.h"
#include "s_util/str.h"

#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"
#include "s_block_device/fat32.h"
#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t pre_al_blocks;
static fat32_device_t *dev;

static bool pretest(void) {
    pre_al_blocks = al_num_user_blocks(get_default_allocator());

    block_device_t *bd = new_da_mem_block_device(FAT32_REQ_SECTOR_SIZE, 2048);
    TEST_TRUE(bd != NULL);

    fernos_error_t err;
    err = init_fat32(bd, 0, 1024, 8);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = parse_new_da_fat32_device(bd, 0, 0, true, &dev);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool posttest(void) {
    delete_fat32_device(dev);

    size_t post_al_blocks = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(pre_al_blocks, post_al_blocks);

    TEST_SUCCEED();
}


bool test_fat32_device_dir_functions(void) {
    BEGIN_SUITE("FAT32 Device Dir Functions");
    return END_SUITE();
}
