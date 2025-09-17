
#include "s_block_device/test/fat32_file_sys.h"
#include "s_block_device/test/file_sys.h"
#include "s_block_device/file_sys.h"
#include "s_block_device/fat32_file_sys.h"
#include "s_util/datetime.h"
#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"

static void now(fernos_datetime_t *dt) {
    // The date time features are optional for the generic FS interface.
    // My FAT32 implementation does use them, however, this test suite will
    // not check at all.
    *dt = (fernos_datetime_t) { 0 };
}

static file_sys_t *gen_fat32_fs(void) {
    fernos_error_t err;

    block_device_t *bd = new_da_mem_block_device(FAT32_REQ_SECTOR_SIZE, 1024);

    if (bd) {
        err = init_fat32(bd, 0, bd_num_sectors(bd), 4);

        file_sys_t *fs;

        if (err == FOS_E_SUCCESS) {
            err = parse_new_da_fat32_file_sys(bd, 0, 0, true, now, &fs);
        }

        if (err == FOS_E_SUCCESS) {
            return fs; // Success!
        }
    }

    // Failure.
    delete_block_device(bd);
    return NULL;
}

bool test_fat32_file_sys(void) {
    return test_file_sys("FAT32 File Sys", gen_fat32_fs);
}

#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t num_user_al_blocks;

static bool pretest(void) {
    num_user_al_blocks = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}

static bool posttest(void) {
    size_t post_num_user_al_blocks = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(num_user_al_blocks, post_num_user_al_blocks);

    TEST_SUCCEED();
}

static bool test_bad_names(void) {
    // Test how the FAT32 FS reacts to the presence of invalid names within the 
    // fat32 device.

    fernos_error_t err;

    block_device_t *bd = new_da_mem_block_device(512, 2048);
    TEST_TRUE(bd != NULL);

    err = init_fat32(bd, 0, bd_num_sectors(bd), 8);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    fat32_device_t *dev;

    err = parse_new_da_fat32_device(bd, 0, 0, false, &dev);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // First we will create a directory with an invalidly named file within.

    uint32_t dir_start;
    err = fat32_new_dir(dev, &dir_start);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    uint32_t dummy_offset;
    fat32_short_fn_dir_entry_t sfn_entry;

    sfn_entry = (fat32_short_fn_dir_entry_t) {
        .short_fn = {'a', ' ', ' ', ' ', ' ' , ' ' , ' ', ' '},
        .extenstion = {' ', ' ', ' '},
        .first_cluster_low = (uint16_t)dir_start,
        .first_cluster_high = (uint16_t)(dir_start >> 16),
        .attrs = FT32F_ATTR_SUBDIR
    };
    err = fat32_new_seq_c8(dev, dev->root_dir_cluster, &sfn_entry, "a", &dummy_offset);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Now add the invalid file.
    uint32_t file_start;
    err = fat32_new_chain(dev, 1, &file_start);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    sfn_entry = (fat32_short_fn_dir_entry_t) {
        .short_fn = {'b', ' ', ' ', ' ', ' ' , ' ' , ' ', ' '},
        .extenstion = {' ', ' ', ' '},
        .first_cluster_low = (uint16_t)file_start,
        .first_cluster_high = (uint16_t)(file_start >> 16),
    };
    err = fat32_new_seq_c8(dev, dir_start, &sfn_entry, "*****", &dummy_offset);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = fat32_flush(dev);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    delete_fat32_device(dev);

    // Only now do we actually create the file system object.

    file_sys_t *fs;

    err = parse_new_da_fat32_file_sys(bd, 0, 0, false, now, &fs);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    fs_node_key_t key;
    err = fs_new_key(fs, NULL, "/a", &key);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    fs_node_info_t info;

    err = fs_get_node_info(fs, key, &info);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_TRUE(info.is_dir);
    TEST_EQUAL_UINT(0, info.len);

    char names_bufs[1][FS_MAX_FILENAME_LEN + 1];
    err = fs_get_child_names(fs, key, 0, 1, names_bufs);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_TRUE(names_bufs[0][0] == '\0');

    fs_delete_key(fs, key);

    fs_node_key_t root_key;
    err = fs_new_key(fs, NULL, "/", &root_key);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = fs_remove(fs, root_key, "a");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    fs_delete_key(fs, root_key);    

    err = fs_flush(fs, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    delete_file_sys(fs);

    delete_block_device(bd);

    TEST_SUCCEED();
}

bool test_fat32_file_sys_impl(void) {
    BEGIN_SUITE("FAT32 FS Impl Specific");
    RUN_TEST(test_bad_names);
    return END_SUITE();
}

