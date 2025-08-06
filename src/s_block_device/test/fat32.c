
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

/**
 * Helper for writing a predefined chain to the FAT.
 */
static bool fat32_store_chain(fat32_device_t *dev, const uint32_t *chain, uint32_t chain_len) {
    fernos_error_t err;

    if (chain_len == 0) {
        TEST_SUCCEED();
    }

    for (uint32_t i = 0; i < chain_len - 1; i++) {
        err =  fat32_set_fat_slot(dev, chain[i], chain[i + 1]);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }
    err = fat32_set_fat_slot(dev, chain[chain_len - 1], FAT32_EOC);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    
    TEST_SUCCEED();
}

static bool test_free_valid_chain(void) {
    fernos_error_t err; 
    uint32_t slot_val;

    const uint32_t chain_indeces[] = {
        4, 6, 9, 5, 3
    };
    const uint32_t chain_indeces_len = sizeof(chain_indeces) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, chain_indeces, chain_indeces_len));

    err = fat32_free_chain(dev, chain_indeces[0]); 
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t i = 0; i < chain_indeces_len; i++) {
        err = fat32_get_fat_slot(dev, chain_indeces[i], &slot_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_UINT(0, slot_val);
    }

    // Lastly, let's just try a single element chain.
    err = fat32_set_fat_slot(dev, 10, FAT32_EOC);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fat32_free_chain(dev, 10); 
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fat32_get_fat_slot(dev, 10, &slot_val);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(0, slot_val);

    TEST_SUCCEED();
}

static bool test_free_bad_chain(void) {
    fernos_error_t err; 

    const uint32_t chain_indeces[] = {
        3, 4, 5
    };
    const uint32_t chain_indeces_len = sizeof(chain_indeces) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, chain_indeces, chain_indeces_len));

    err = fat32_set_fat_slot(dev, chain_indeces[chain_indeces_len - 1], FAT32_BAD_CLUSTER);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    
    err = fat32_free_chain(dev, chain_indeces[0]); 
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    TEST_SUCCEED();
}


bool test_fat32_device(void) {
    BEGIN_SUITE("FAT32 Device");
    RUN_TEST(test_get_set_fat_slots);
    RUN_TEST(test_free_valid_chain);
    RUN_TEST(test_free_bad_chain);
    return END_SUITE();
}



