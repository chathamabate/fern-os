
#include "s_block_device/test/file_sys.h"

#include "k_bios_term/term.h"
#include "s_mem/allocator.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static file_sys_t *(*generate_fs)(void) = NULL;
static file_sys_t *fs = NULL;
static size_t num_al_blocks = 0;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_TRUE(generate_fs != NULL);
    
    fs = generate_fs();
    TEST_TRUE(fs != NULL);

    TEST_SUCCEED();
}

static bool posttest(void) {
    fernos_error_t err;

    err = fs_flush(fs, NULL);
    TEST_EQUAL_UINT(FOS_SUCCESS, err);

    delete_file_sys(fs);

    size_t curr_user_blocks = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(num_al_blocks, curr_user_blocks);

    TEST_SUCCEED();
}

static bool test_fs_touch_and_mkdir(void) {
    fernos_error_t err;

    // Maybe we could make a could of subdirectories on root?
    // Put files in those subdirectories???

    fs_node_key_t root_key;

    err = fs_new_key(fs, NULL, "/", &root_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    char name_buf[2] = " ";

    // Make a bunch of nested subdirectories.
    for (char dir0_name = 'a'; dir0_name <= 'e'; dir0_name++) {
        name_buf[0] = dir0_name;

        fs_node_key_t dir0_key;
        err = fs_mkdir(fs, root_key, name_buf, &dir0_key);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (char dir1_name = 'a'; dir1_name <= 'e'; dir1_name++) {
            name_buf[0] = dir1_name;

            fs_node_key_t dir1_key;

            err = fs_mkdir(fs, dir0_key, name_buf, &dir1_key);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            for (char file_name = 'A'; file_name <= 'E'; file_name++) {
                name_buf[0] = file_name;

                err = fs_touch(fs, dir1_key, name_buf, NULL);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);
            }

            fs_delete_key(fs, dir1_key);
        }

        fs_delete_key(fs, dir0_key);
    }

    // Ok, now let's try access a series of expected and unexpected paths.

    const char *existing_paths[] = {
        "./a",
        "/a",
        "d",
        "/a/b/C",
        "b/c/",
        "./a/../b/.././c/b/E"
    };
    const size_t num_existing_paths = sizeof(existing_paths) / sizeof(existing_paths[0]);

    fs_node_key_t key;

    for (size_t i = 0; i < num_existing_paths; i++) {
        err = fs_new_key(fs, root_key, existing_paths[i], &key);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        fs_delete_key(fs, key);
    }

    const char *non_existing_paths[] = {
        "f",
        "/f",
        "/a/b/G",
        "..", // The root dir doesn't have a parent!
        "a/b/../c/9",
        "/c/f/B"
    };
    const size_t num_non_existing_paths = sizeof(non_existing_paths) / sizeof(non_existing_paths[0]);

    for (size_t i = 0; i < num_non_existing_paths; i++) {
        err = fs_new_key(fs, root_key, non_existing_paths[i], &key);
        TEST_EQUAL_HEX(FOS_EMPTY, err);
    }

    // Finally, let's try out some simple error cases.

    // 1) trying to create a file/directory into a non-sub directory should fail!

    err = fs_new_key(fs, NULL, "/a/b/C", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch(fs, key, "k", NULL);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    err = fs_mkdir(fs, key, "k", NULL);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    fs_delete_key(fs, key);

    // 2) We should not be able to make a file if one with the same name already exists.

    err = fs_new_key(fs, NULL, "/b/b", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch(fs, key, "A", NULL);
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    err = fs_mkdir(fs, key, "A", NULL);
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    // Maybe we can test exhausting all disk space in a later test.

    fs_delete_key(fs, key);

    fs_delete_key(fs, root_key);

    TEST_SUCCEED();
}


bool test_file_sys(const char *name, file_sys_t *(*gen)(void)) {
    generate_fs = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_fs_touch_and_mkdir);
    return END_SUITE();
}
