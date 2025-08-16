
#include "s_block_device/test/fat32.h"

#include <stdbool.h>

#include "s_util/rand.h"
#include "s_util/str.h"

#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"
#include "s_block_device/fat32.h"
#include "s_block_device/fat32_dir.h"
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

static bool test_fat32_new_dir(void) {
    fernos_error_t err;

    uint32_t slot_ind;

    err = fat32_new_dir(dev, &slot_ind);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fat32_dir_entry_t entry;

    err = fat32_read_dir_entry(dev, slot_ind, 0, &entry);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_EQUAL_HEX(FAT32_DIR_ENTRY_TERMINTAOR, entry.raw[0]);

    TEST_SUCCEED();
}

static bool test_fat32_read_write_dir_entry(void) {
    fernos_error_t err;

    uint32_t slot_ind;

    err = fat32_new_dir(dev, &slot_ind);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fat32_dir_entry_t entry;

    uint32_t num_entries = 0;
    while (true) {
        err = fat32_read_dir_entry(dev, slot_ind, num_entries, &entry);
        if (err == FOS_INVALID_INDEX) {
            break;
        }
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        num_entries++;
    }

    for (uint32_t i = 0; i < num_entries; i++) {
        mem_set(&entry, (uint8_t)i, sizeof(fat32_dir_entry_t));
        err = fat32_write_dir_entry(dev, slot_ind, i, &entry);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    fat32_dir_entry_t other;

    for (uint32_t i = 0; i < num_entries; i++) {
        err = fat32_read_dir_entry(dev, slot_ind, i, &entry);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        mem_set(&other, (uint8_t)i, sizeof(fat32_dir_entry_t));
        TEST_TRUE(mem_cmp(&entry, &other, sizeof(fat32_dir_entry_t)));
    }

    TEST_SUCCEED();
}


bool test_fat32_device_dir_functions(void) {
    BEGIN_SUITE("FAT32 Device Dir Functions");
    RUN_TEST(test_fat32_new_dir);
    RUN_TEST(test_fat32_read_write_dir_entry);
    return END_SUITE();
}
