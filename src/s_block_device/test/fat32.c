
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
    err = init_fat32(bd, 0, 1028, 4);
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

/**
 * Succeeds when the given chain has the given length.
 */
static bool fat32_expect_len(fat32_device_t *dev, uint32_t slot_ind, uint32_t len) {
    fernos_error_t err;

    uint32_t visited = 0;
    uint32_t iter = slot_ind;

    while (true) {
        uint32_t next_iter;
        err = fat32_get_fat_slot(dev, iter, &next_iter);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        visited++;

        if (visited == len) {
            TEST_TRUE(FAT32_IS_EOC(next_iter));
            TEST_SUCCEED();
        }

        iter = next_iter;
    }
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

static bool test_pop_free_fat_slot(void) {
    fernos_error_t err;

    while (true) {
        uint32_t slot_ind;

        err = fat32_pop_free_fat_slot(dev, &slot_ind);

        if (err == FOS_NO_SPACE) {
            TEST_SUCCEED();
        }

        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_TRUE(slot_ind > 2 && slot_ind < dev->num_fat_slots);

        uint32_t slot_val;

        err = fat32_get_fat_slot(dev, slot_ind, &slot_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_HEX(FAT32_EOC, slot_val);
    }

    TEST_SUCCEED();
}

static bool test_new_chain(void) {
    fernos_error_t err;

    uint32_t slot_ind;

    // Try a bunch of different lengths.
    for (uint32_t len = 1; len < 25; len += 3) {
        err = fat32_new_chain(dev, len, &slot_ind);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(fat32_expect_len(dev, slot_ind, len));

        err = fat32_free_chain(dev, slot_ind);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    TEST_SUCCEED();
}

static bool test_new_chain_big(void) {
    // Let's just try out exhausting the number of chains?
    
    fernos_error_t err;

    uint32_t slot_ind;

    err = fat32_new_chain(dev, dev->num_fat_slots, &slot_ind);
    TEST_EQUAL_HEX(FOS_NO_SPACE, err);
    
    // Now try making a bunch of small chains until we run out of space.
    while (true) {
        err = fat32_new_chain(dev, 5, &slot_ind);
        if (err == FOS_NO_SPACE) {
            TEST_SUCCEED();
        }

        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    TEST_SUCCEED();
}

static bool test_extend_chain(void) {
    fernos_error_t err;

    const uint32_t init_chain[] = {
        3, 5, 16, 4, 6
    };
    const uint32_t init_chain_len = sizeof(init_chain) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, init_chain, init_chain_len));

    uint32_t new_len = init_chain_len + 5;
    err = fat32_resize_chain(dev, init_chain[0], new_len);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(fat32_expect_len(dev, init_chain[0], new_len));

    new_len += 3;
    err = fat32_resize_chain(dev, init_chain[0], new_len);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(fat32_expect_len(dev, init_chain[0], new_len));

    // This one should fail!

    err = fat32_resize_chain(dev, init_chain[0], new_len + dev->num_fat_slots);
    TEST_EQUAL_HEX(FOS_NO_SPACE, err);

    TEST_TRUE(fat32_expect_len(dev, init_chain[0], new_len));

    TEST_SUCCEED();
}

static bool test_shrink_chain(void) {
    fernos_error_t err;

    const uint32_t init_chain[] = {
        6, 9, 16, 5, 19, 17, 15
    };
    const uint32_t init_chain_len = sizeof(init_chain) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, init_chain, init_chain_len));

    TEST_TRUE(init_chain_len > 3);
    uint32_t new_len = init_chain_len - 3;
    
    err = fat32_resize_chain(dev, init_chain[0], new_len);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(fat32_expect_len(dev, init_chain[0], new_len));

    // Now let's try freeing the chain.
    err = fat32_resize_chain(dev, init_chain[0], 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t i = 0; i < init_chain_len; i++) {
        uint32_t slot_val;

        err = fat32_get_fat_slot(dev, init_chain[i], &slot_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_EQUAL_UINT(0, slot_val);
    }

    TEST_SUCCEED();
}

static bool test_resize_malformed(void) {
    fernos_error_t err;

    const uint32_t init_chain[] = {
        6, 8, 11, 23, 19, 17, 15, 4, 5
    };
    const uint32_t init_chain_len = sizeof(init_chain) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, init_chain, init_chain_len));

    err = fat32_set_fat_slot(dev, init_chain[init_chain_len - 1],  FAT32_BAD_CLUSTER);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fat32_resize_chain(dev, init_chain[0], init_chain_len + 5);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // The behavior of shrinking a malformed chain not the well defined tbh.
    // err = fat32_resize_chain(dev, init_chain[0], init_chain_len - 1);
    // TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);
    
    TEST_SUCCEED();
}

static bool test_traverse_chain(void) {
    fernos_error_t err;

    const uint32_t init_chain[] = {
        30, 23, 40, 12, 5, 6, 19, 4
    };
    const uint32_t init_chain_len = sizeof(init_chain) / sizeof(uint32_t);

    TEST_TRUE(fat32_store_chain(dev, init_chain, init_chain_len));

    uint32_t traversed_ind;

    for (uint32_t i = 0; i < init_chain_len; i++) {

        err = fat32_traverse_chain(dev, init_chain[0], i, &traversed_ind);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_EQUAL_UINT(init_chain[i], traversed_ind);
    }

    err = fat32_traverse_chain(dev, init_chain[0], init_chain_len, &traversed_ind);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // Now malform the chain.

    err = fat32_set_fat_slot(dev, init_chain[2], FAT32_BAD_CLUSTER);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fat32_traverse_chain(dev, init_chain[0], 1, &traversed_ind);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(init_chain[1], traversed_ind);

    // This is kinda confusing, but we are allowed to traverse the chain even up to a malformed
    // cluster. An error is only returned if a bad cluster prevents us from being able to
    // reach our desired offset in the chain.
    err = fat32_traverse_chain(dev, init_chain[0], 2, &traversed_ind);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(init_chain[2], traversed_ind);

    // i.e. here.
    err = fat32_traverse_chain(dev, init_chain[0], 3, &traversed_ind);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    TEST_SUCCEED();
}

static bool test_read_write(void) {
    fernos_error_t err;

    uint32_t chain_clusters = 5;
    uint32_t chain;

    err = fat32_new_chain(dev, chain_clusters, &chain);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint32_t chain_sectors = chain_clusters * dev->sectors_per_cluster;

    uint8_t sector_buf[2][FAT32_REQ_SECTOR_SIZE];

    // Maybe just a simple write and read first?

    for (uint32_t i = 0; i < chain_sectors; i++) {
        mem_set(sector_buf[0], (uint8_t)i, FAT32_REQ_SECTOR_SIZE);

        err = fat32_write(dev, chain, i, 1, sector_buf[0]);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    for (uint32_t i = 0; i < chain_sectors; i++) {
        err = fat32_read(dev, chain, i, 1, sector_buf[0]);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        mem_set(sector_buf[1], (uint8_t)i, FAT32_REQ_SECTOR_SIZE);
        TEST_TRUE(mem_cmp(sector_buf[0], sector_buf[1], FAT32_REQ_SECTOR_SIZE));
    }

    // Ok, now we are going to overwrie then read again.

    uint8_t *full_buf = da_malloc(FAT32_REQ_SECTOR_SIZE * chain_sectors);
    TEST_TRUE(full_buf != NULL);

    for (uint32_t i = 0; i < chain_sectors; i++) {
        mem_set(full_buf, i, (chain_sectors - i) * FAT32_REQ_SECTOR_SIZE);
        err = fat32_write(dev, chain, i, chain_sectors - i, full_buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // The resulting state of the chain should be the same as before after that above writes.

    for (uint32_t i = 0; i < chain_sectors; i++) {
        err = fat32_read(dev, chain, i, 1, sector_buf[0]);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        mem_set(sector_buf[1], (uint8_t)i, FAT32_REQ_SECTOR_SIZE);
        TEST_TRUE(mem_cmp(sector_buf[0], sector_buf[1], FAT32_REQ_SECTOR_SIZE));
    }

    da_free(full_buf);

    TEST_SUCCEED();
}

static bool test_read_write_multi_chain(void) {
    fernos_error_t err;

    // Here we have a few chains which are kinda interleved.
    const uint32_t chains[][6] = {
        {4, 6, 8, 9, 11, 15},
        {5, 10, 14, 13, 12, 20},
        {7, 19, 16, 18, 17, 21},
    };
    const uint32_t num_chains = sizeof(chains) / sizeof(chains[0]);
    const uint32_t clusters_per_chain = sizeof(chains[0]) / sizeof(uint32_t);
    const uint32_t sectors_per_chain = dev->sectors_per_cluster * clusters_per_chain;

    for (uint32_t i = 0; i < num_chains; i++) {
        TEST_TRUE(fat32_store_chain(dev, chains[i], clusters_per_chain));
    }

    uint8_t *buf = da_malloc(sectors_per_chain * FAT32_REQ_SECTOR_SIZE);
    TEST_TRUE(buf != NULL);

    // Ok, now we are just testing if writing to one chain potentially corrupts another.

    for (uint32_t i = 0; i < num_chains; i++) {
        mem_set(buf, i, sectors_per_chain * FAT32_REQ_SECTOR_SIZE);

        err = fat32_write(dev, chains[i][0], 0, sectors_per_chain, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    for (uint32_t i = 0; i < num_chains; i++) {
        err = fat32_read(dev, chains[i][0], 0, sectors_per_chain, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (uint32_t j = 0; j < sectors_per_chain * FAT32_REQ_SECTOR_SIZE; j++) {
            TEST_EQUAL_UINT(i, buf[j]);
        }
    }

    da_free(buf);

    TEST_SUCCEED();
}

static bool test_read_write_bad(void) {
    fernos_error_t err;

    const uint32_t chain[] = {
        5, 9, 11, 6
    };
    const uint32_t chain_len = sizeof(chain) / sizeof(uint32_t);
    const uint32_t chain_sectors = chain_len * dev->sectors_per_cluster;

    TEST_TRUE(fat32_store_chain(dev, chain, chain_len));

    uint8_t *buf = da_malloc(chain_sectors * FAT32_REQ_SECTOR_SIZE);
    TEST_TRUE(buf != NULL);

    err = fat32_write(dev, chain[0], 0, chain_sectors + 1, buf);
    TEST_EQUAL_HEX(FOS_INVALID_RANGE, err);

    err = fat32_read(dev, chain[0], 0, chain_sectors + 1, buf);
    TEST_EQUAL_HEX(FOS_INVALID_RANGE, err);

    err = fat32_write(dev, chain[0], chain_sectors, 1, buf);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = fat32_read(dev, chain[0], chain_sectors, 1, buf);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // Ok, now let's try to malform the chain.

    err = fat32_set_fat_slot(dev, chain[1], FAT32_BAD_CLUSTER);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // A range which contains a bad sector.
    err = fat32_write(dev, chain[0], 0, chain_sectors, buf);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    err = fat32_read(dev, chain[0], 0, chain_sectors, buf);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Indexing after a bad cluster.
    err = fat32_write(dev, chain[0], chain_sectors - 1, 1, buf);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    err = fat32_read(dev, chain[0], chain_sectors - 1, 1, buf);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    da_free(buf);

    TEST_SUCCEED();
}

static bool test_read_write_piece(void) {
    TEST_SUCCEED();
}

// This will likely be adjusted to include read/write_piece functions.
static bool test_read_write_random(void) {
    // Ok, now we could try maybe a more funky read write combination??

    // Oooh, maybe a random test!

    fernos_error_t err;

    rand_t r = rand(0);

    const uint32_t num_chain_clusters = 24;
    const uint32_t num_chain_sectors = dev->sectors_per_cluster * num_chain_clusters;

    uint32_t chain;
    err = fat32_new_chain(dev, num_chain_clusters, &chain);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // The control buffer is what will be compared against during the test.
    uint8_t *cntl_buf = da_malloc(num_chain_sectors * FAT32_REQ_SECTOR_SIZE);
    mem_set(cntl_buf, 0, num_chain_sectors * FAT32_REQ_SECTOR_SIZE);

    // Now make the control sectors and the chain sectors equal.
    err = fat32_write(dev, chain, 0, num_chain_sectors, cntl_buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    const uint32_t max_trial_sectors = 8;
    const uint32_t random_trials = 150;

    uint8_t *trial_buf = da_malloc(max_trial_sectors * FAT32_REQ_SECTOR_SIZE);
    TEST_TRUE(trial_buf != NULL);

    mem_set(trial_buf, 0, max_trial_sectors * FAT32_REQ_SECTOR_SIZE);

    for (uint32_t rt = 0; rt < random_trials; rt++) {
        const uint32_t sector_offset = next_rand_u32(&r) % max_trial_sectors;
        const uint32_t trial_sectors = next_rand_u32(&r) % (max_trial_sectors - sector_offset);

        if (next_rand_u1(&r)) { // Write
            const uint8_t rand_val = next_rand_u8(&r);

            mem_set(trial_buf, rand_val, trial_sectors * FAT32_REQ_SECTOR_SIZE);

            // Write to control.
            mem_cpy(cntl_buf + (sector_offset * FAT32_REQ_SECTOR_SIZE), trial_buf, 
                    trial_sectors * FAT32_REQ_SECTOR_SIZE);

            // Write to device.
            err = fat32_write(dev, chain, sector_offset, trial_sectors, trial_buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        } else { // Read
            err = fat32_read(dev, chain, sector_offset, trial_sectors, trial_buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            TEST_TRUE(mem_cmp(cntl_buf + (sector_offset * FAT32_REQ_SECTOR_SIZE), 
                        trial_buf, trial_sectors * FAT32_REQ_SECTOR_SIZE));
        }
    }

    da_free(trial_buf);
    da_free(cntl_buf);

    TEST_SUCCEED();
}

// Ok, now what, read/write piece???

bool test_fat32_device(void) {
    BEGIN_SUITE("FAT32 Device");
    RUN_TEST(test_get_set_fat_slots);
    RUN_TEST(test_free_valid_chain);
    RUN_TEST(test_free_bad_chain);
    RUN_TEST(test_pop_free_fat_slot);
    RUN_TEST(test_new_chain);
    RUN_TEST(test_new_chain_big);
    RUN_TEST(test_extend_chain);
    RUN_TEST(test_shrink_chain);
    RUN_TEST(test_resize_malformed);
    RUN_TEST(test_traverse_chain);
    RUN_TEST(test_read_write);
    RUN_TEST(test_read_write_multi_chain);
    RUN_TEST(test_read_write_bad);

    RUN_TEST(test_read_write_random);
    return END_SUITE();
}



