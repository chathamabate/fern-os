
#include "s_block_device/test/fat32.h"

#include <stdbool.h>

#include "s_util/rand.h"
#include "s_util/str.h"

#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"
#include "s_block_device/fat32.h"
#include "s_block_device/fat32_dir.h"
#include "k_bios_term/term.h"
#include "s_util/rand.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t pre_al_blocks;
static fat32_device_t *dev;
static uint32_t slot_ind; // Index of the directory.

static bool pretest(void) {
    pre_al_blocks = al_num_user_blocks(get_default_allocator());

    block_device_t *bd = new_da_mem_block_device(FAT32_REQ_SECTOR_SIZE, 2048);
    TEST_TRUE(bd != NULL);

    fernos_error_t err;
    err = init_fat32(bd, 0, 1024, 8);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = parse_new_da_fat32_device(bd, 0, 0, true, &dev);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fat32_new_dir(dev, &slot_ind);
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

    fat32_dir_entry_t entry;

    // This just tests the the call to new_dir from within the pretest outputs a new
    // directory with the expected format.

    err = fat32_read_dir_entry(dev, slot_ind, 0, &entry);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_EQUAL_HEX(FAT32_DIR_ENTRY_TERMINTAOR, entry.raw[0]);

    TEST_SUCCEED();
}

static bool test_fat32_read_write_dir_entry(void) {
    fernos_error_t err;

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

static bool test_fat32_get_free_seq(void) {
    fernos_error_t err;

    fat32_dir_entry_t dummy_entry = {
        .short_fn = {
            .short_fn = {'D', 'U', 'M', 'M', 'Y', ' ', ' ', ' '},
            .extenstion = {' ' , ' ', ' '},
            .attrs = 0
        }
    };

    rand_t r = rand(0);

    // What we are going to do is to get a bunch of sequences of certain lengths.
    // Each time, we will randomly place a SFN entry in one of the sequences 
    // entries.

    for (uint32_t seq_len = 1; seq_len < 10; seq_len += 3) {
        uint32_t seq_start;
        err = fat32_get_free_seq(dev, slot_ind, seq_len, &seq_start);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Now confirm that this entire sequence is actually unused!

        for (uint32_t i = 0; i < seq_len; i++) {
            fat32_dir_entry_t entry;

            err = fat32_read_dir_entry(dev, slot_ind, seq_start + i, &entry);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            TEST_EQUAL_HEX(FAT32_DIR_ENTRY_UNUSED, entry.raw[0]);
        }

        uint32_t rand_ind = next_rand_u32(&r) % seq_len;
        err = fat32_write_dir_entry(dev, slot_ind, seq_start + rand_ind, &dummy_entry);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    TEST_SUCCEED();
}

/*
 * NOTE: some of the behaviors are kinda intertwined. It's kinda hard to test one
 * behavior in isolation. So, the "dir_ops" tests are just tests which test a bunch
 * of endpoints together.
 */

static bool test_fat32_dir_ops0(void) {
    // This will make a few sequences, then erase a few??

    fernos_error_t err;

    fat32_short_fn_dir_entry_t sfn_entry = {
        .short_fn = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .extenstion = {'T', ' ', ' '},
        .attrs = 0
    };
    uint16_t lfn_buf[FAT32_MAX_LFN_LEN + 1];

    uint32_t entry_offsets[20];
    const uint32_t num_entry_offsets = sizeof(entry_offsets) / sizeof(uint32_t);
    TEST_TRUE(num_entry_offsets <= 26); // We are going to be doing some basic alphabetical stuff.

    for (uint32_t i = 0; i < num_entry_offsets; i++) {
        const uint32_t lfn_len = (i + 1);
        for (uint32_t j = 0; j < lfn_len; j++) {
            lfn_buf[j] = (uint16_t)('a' + j);
        }
        lfn_buf[lfn_len] = 0;

        sfn_entry.short_fn[0] = 'A' + i;
        
        uint32_t entry_offset;
        err = fat32_new_seq(dev, slot_ind, &sfn_entry, lfn_buf, &entry_offset);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        entry_offsets[i] = entry_offset;
    }

    // Free some random seqences.
    rand_t r = rand(0);
    for (uint32_t i = 0; i < num_entry_offsets / 2; i++) {
        const uint32_t entry_to_free = next_rand_u32(&r) % num_entry_offsets;
        if (entry_offsets[entry_to_free] != 0xFFFFFFFF) {
            err = fat32_erase_seq(dev, slot_ind, entry_offsets[entry_to_free]);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            entry_offsets[entry_to_free] = 0xFFFFFFFF;
        }
    }

    // Ok now we are going for the Big kahuna!
    for (uint32_t i = 0; i < FAT32_MAX_LFN_LEN; i++) {
        lfn_buf[i] = 'a' + (i % 26);
    }
    lfn_buf[FAT32_MAX_LFN_LEN] = 0;

    for (uint32_t i = 0; i < num_entry_offsets; i++) {
        if (entry_offsets[i] == 0xFFFFFFFF) {
            err = fat32_new_seq(dev, slot_ind, &sfn_entry, lfn_buf, (entry_offsets + i));
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }
    }

    // FINALLY, let's iterate over the directory using the next_seq call and confirm all
    // sequences are valid.

    uint32_t entry_iter = 0;
    uint32_t seq_start;

    for (uint32_t i = 0; i < num_entry_offsets; i++) {
        err = fat32_next_dir_seq(dev, slot_ind, entry_iter, &seq_start);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Ok, let's get the SFN now.
        uint32_t sfn_offset;
        err = fat32_get_dir_seq_sfn(dev, slot_ind, seq_start, &sfn_offset);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = fat32_get_dir_seq_lfn(dev, slot_ind, sfn_offset, lfn_buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (uint32_t j = 0; lfn_buf[j] != 0; j++) {
            TEST_TRUE(lfn_buf[j] == 'a' + (j % 26));
        }

        entry_iter = sfn_offset + 1;
    }

    err = fat32_next_dir_seq(dev, slot_ind, entry_iter, &seq_start);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    TEST_SUCCEED();
}

static bool test_fat32_dir_ops1(void) {
    fernos_error_t err;

    fat32_dir_entry_t temp_entry;

    fat32_short_fn_dir_entry_t sfn_entry = {
        .short_fn = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .extenstion = {' ' , ' ', ' '},
        .attrs = 0
    };
    uint16_t lfn_buf[FAT32_MAX_LFN_LEN + 1];

    uint32_t seq_starts[100];
    mem_set(seq_starts, 0xFF, sizeof(seq_starts));

    const uint32_t num_seq_starts = sizeof(seq_starts) / sizeof(uint32_t);

    rand_t r = rand(0);

    for (uint32_t i = 0; i < num_seq_starts * 30; i++) {
        const uint32_t seq_starts_ind = next_rand_u32(&r) % num_seq_starts;

        if (seq_starts[seq_starts_ind] == 0xFFFFFFFF) {
            // If we hit an unused cell, allocate!
            
            // Ok, we are going to generate a SFN and LFN from the seq_starts_ind.
            
            const uint32_t lfn_len = next_rand_u32(&r) % (FAT32_MAX_LFN_LEN + 1);
            for (uint32_t ci = 0; ci < lfn_len; ci++) {
                lfn_buf[ci] = 'a' + (seq_starts_ind % 26);
            }
            lfn_buf[lfn_len] = 0;

            for (uint32_t ci = 0; ci < sizeof(sfn_entry.short_fn); ci++) {
                sfn_entry.short_fn[ci] = 'A' + (seq_starts_ind % 26);
            }

            err = fat32_new_seq(dev, slot_ind, &sfn_entry, lfn_buf, seq_starts + seq_starts_ind);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            // Here, let's confirm the entire sequence is actually created correctly.
            const uint32_t lfn_entries = FAT32_NUM_LFN_ENTRIES(lfn_len);
            for (uint32_t i = 0; i < lfn_entries; i++) {
                err = fat32_read_dir_entry(dev, slot_ind, seq_starts[seq_starts_ind] + i, &temp_entry);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);
                
                TEST_EQUAL_HEX((i == 0 ? FAT32_LFN_START_PREFIX | lfn_entries : lfn_entries - i), temp_entry.raw[0]);
                TEST_TRUE(FT32F_ATTR_IS_LFN(temp_entry.short_fn.attrs));
            }



        } else {
            // If we hit a used cell, erase!

            // First let's confirm this sequence still holds the expected values.
            
            uint32_t sfn_offset;
            err = fat32_get_dir_seq_sfn(dev, slot_ind, seq_starts[seq_starts_ind], &sfn_offset);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            err = fat32_read_dir_entry(dev, slot_ind, sfn_offset, (fat32_dir_entry_t *)&sfn_entry);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            for (uint32_t ci = 0; ci < sizeof(sfn_entry.short_fn); ci++) {
                TEST_EQUAL_HEX((char)('A' + (seq_starts_ind % 26)), sfn_entry.short_fn[ci]);
            }

            // Confirm this name is marked IN_USE by the check sfn function.
            err = fat32_check_sfn(dev, slot_ind, sfn_entry.short_fn, "   ");
            TEST_EQUAL_HEX(FOS_IN_USE, err);

            err = fat32_get_dir_seq_lfn(dev, slot_ind, sfn_offset, lfn_buf);
            TEST_TRUE(err == FOS_SUCCESS || err == FOS_EMPTY);

            // Some sequences may just have an SFN and no LFN!
            if (err == FOS_SUCCESS) {
                for (uint32_t ci = 0; lfn_buf[ci] != 0; ci++) {
                    TEST_EQUAL_HEX((char)('a' + (seq_starts_ind % 26)), lfn_buf[ci]);
                }

                // Confirm the lfn is marked in use by the check lfn funciton.
                err = fat32_check_lfn(dev, slot_ind, lfn_buf);
                TEST_EQUAL_HEX(FOS_IN_USE, err);
            }

            // Finally erase our sequence.
            err = fat32_erase_seq(dev, slot_ind, seq_starts[seq_starts_ind]);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            seq_starts[seq_starts_ind] = 0xFFFFFFFF;
        }
    }

    TEST_SUCCEED();
}


bool test_fat32_device_dir_functions(void) {
    BEGIN_SUITE("FAT32 Device Dir Functions");
    RUN_TEST(test_fat32_new_dir);
    RUN_TEST(test_fat32_read_write_dir_entry);
    RUN_TEST(test_fat32_get_free_seq);
    RUN_TEST(test_fat32_dir_ops0);
    RUN_TEST(test_fat32_dir_ops1);
    return END_SUITE();
}
