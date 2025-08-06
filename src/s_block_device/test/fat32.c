
#include "s_block_device/test/fat32.h"

#include <stdbool.h>

#include "s_util/rand.h"

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
    err = init_fat32(bd, 0, 2048, 4);
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

static bool test_get_set_fat_slots(void) {
    fernos_error_t err;
    uint32_t slot_val;

    for (uint32_t i = 0; i < dev->num_fat_slots; i += 3) {
        uint32_t screwed_up_ind = i | ((i % 16) << 28);
        uint32_t screwed_up_val = i | (((i % 16) + 2) << 28);
        err = fat32_set_fat_slot(dev, screwed_up_ind, screwed_up_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    for (uint32_t i = 0; i < dev->num_fat_slots; i += 3) {
        uint32_t screwed_up_ind = i | (((i+3) % 16) << 28);
        err = fat32_get_fat_slot(dev, screwed_up_ind, &slot_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_EQUAL_UINT(i, slot_val);
    }

    // It's kinda hard to find a test to fit this into. Just gonna put this here and make sure it
    // succeeds.
    err = fat32_sync_fats(dev);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_FALSE(fat32_set_fat_slot(dev, dev->num_fat_slots, 0) == FOS_SUCCESS);
    TEST_FALSE(fat32_set_fat_slot(dev, FAT32_BAD_CLUSTER, 0) == FOS_SUCCESS);
    TEST_FALSE(fat32_set_fat_slot(dev,0xFFFFFFFF, 0) == FOS_SUCCESS);

    TEST_FALSE(fat32_get_fat_slot(dev, dev->num_fat_slots, &slot_val) == FOS_SUCCESS);
    TEST_FALSE(fat32_get_fat_slot(dev, FAT32_BAD_CLUSTER, &slot_val) == FOS_SUCCESS);
    TEST_FALSE(fat32_get_fat_slot(dev,0xFFFFFFFF, &slot_val) == FOS_SUCCESS);

    TEST_SUCCEED();
}

static bool test_free_chain(void) {

    TEST_SUCCEED();
}

// I'm not really sure how to test some of this chain stuff IMO.

bool test_fat32_device(void) {
    BEGIN_SUITE("FAT32 Device");
    RUN_TEST(test_get_set_fat_slots);
    RUN_TEST(test_free_chain);
    return END_SUITE();
}



