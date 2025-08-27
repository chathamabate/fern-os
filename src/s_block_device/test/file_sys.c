
#include "s_block_device/test/file_sys.h"

#include "k_bios_term/term.h"
#include "s_mem/allocator.h"
#include "s_util/str.h"
#include "s_util/rand.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static file_sys_t *(*generate_fs)(void) = NULL;
static file_sys_t *fs = NULL;
static size_t num_al_blocks = 0;
static fs_node_key_t root_key = NULL;

static bool pretest(void) {
    fernos_error_t err;

    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_TRUE(generate_fs != NULL);
    
    fs = generate_fs();
    TEST_TRUE(fs != NULL);

    err = fs_new_key(fs, NULL, "/", &root_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool posttest(void) {
    fernos_error_t err;

    fs_delete_key(fs, root_key);

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
        TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);
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

static bool test_fs_remove(void) {
    fernos_error_t err;

    fs_node_key_t subdir_key;
    err = fs_mkdir(fs, root_key, "sub", &subdir_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    char name_buf[2] = " ";
    for (char c = 'a'; c <= 'c'; c++) {
        name_buf[0] = c;

        err = fs_touch(fs, subdir_key, name_buf, NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // At this point, we should not be able to delete the subdir.

    err = fs_remove(fs, root_key, "sub");
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    err = fs_remove(fs, subdir_key, "a");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_node_key_t dummy_key;
    err = fs_new_key(fs, subdir_key, "a", &dummy_key);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = fs_remove(fs, subdir_key, "b");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_remove(fs, subdir_key, "c");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_delete_key(fs, subdir_key);

    err = fs_remove(fs, root_key, "sub");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_remove(fs, root_key, "sub");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    TEST_SUCCEED();
}

static bool test_fs_get_child_names(void) {
    fernos_error_t err;

    char names_bufs[3][FS_MAX_FILENAME_LEN + 1];
    const size_t num_names_bufs = sizeof(names_bufs) / sizeof(names_bufs[0]);

    char name_buf[2] = " ";

    const size_t num_files = 10;
    for (size_t i = 0; i < num_files; i++) {
        name_buf[0] = 'a' + i;

        err = fs_touch(fs, root_key, name_buf, NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    for (size_t i = 0; i < num_files; i += num_names_bufs) {
        err = fs_get_child_names(fs, root_key, i, num_names_bufs, names_bufs);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (size_t j = 0; j < num_names_bufs; j++) {
            if (i + j < num_files) {
                name_buf[0] = 'a' + i + j;
                TEST_TRUE(str_eq(name_buf, names_bufs[j]));
            } else {
                TEST_TRUE(str_eq("", names_bufs[j]));
            }
        }
    }

    err = fs_get_child_names(fs, root_key, num_files, num_names_bufs, names_bufs);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0; i < num_names_bufs; i++) {
        TEST_TRUE(str_eq("", names_bufs[i]));
    }

    // Now let's try getting the names from a file (which should fail)

    fs_node_key_t file_key;

    err = fs_new_key(fs, root_key, "a", &file_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_get_child_names(fs, file_key, 0, num_names_bufs, names_bufs);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    fs_delete_key(fs, file_key);

    TEST_SUCCEED();
}

static bool test_fs_get_node_info(void) {
    fernos_error_t err;

    fs_node_info_t info;

    err = fs_get_node_info(fs, root_key, &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(info.is_dir);
    TEST_EQUAL_UINT(0, info.len);

    err = fs_touch(fs, root_key, "a", NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_node_key_t child_key;

    err = fs_touch(fs, root_key, "b", &child_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_get_node_info(fs, child_key, &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_FALSE(info.is_dir);
    TEST_EQUAL_UINT(0, info.len); // Files should start with len 0.

    fs_delete_key(fs, child_key);

    err = fs_mkdir(fs, root_key, "c", &child_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_get_node_info(fs, child_key, &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(info.is_dir);
    TEST_EQUAL_UINT(0, info.len); // Make sure "." and ".." are skipped.

    fs_delete_key(fs, child_key);

    // Finally go back to root dir now that it has children.

    err = fs_get_node_info(fs, root_key, &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(info.is_dir);
    TEST_EQUAL_UINT(3, info.len);

    TEST_SUCCEED();
}

static bool test_fs_rw0(void) {
    fernos_error_t err;

    // Pretty simple write than read.
    fs_node_key_t key;

    err = fs_touch(fs, root_key, "a.txt", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t buf[100];

    const size_t file_size = sizeof(buf);
    err = fs_resize(fs, key, file_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0; i < file_size; i++) {
        buf[i] = (uint8_t)i;
    }

    err = fs_write(fs, key, 0, file_size, buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    mem_set(buf, 0, file_size);

    const size_t skip = 11;
    for (size_t i = 0; i < file_size; i += skip) {
        const size_t read_amt = i + skip > file_size ? file_size - i : skip;
        err = fs_read(fs, key, i, read_amt, buf);
        for (size_t j = 0; j < read_amt; j++) {
            TEST_EQUAL_UINT(i + j, buf[j]);
        }
    }

    fs_delete_key(fs, key);

    // Throwing this in real quick here.
    err = fs_write(fs, root_key, 0, 10, buf);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    TEST_SUCCEED();
}

static bool test_fs_rw1(void) {
    // IDK, maybe try a bigger file now??
    fernos_error_t err;

    // Pretty simple write than read.

    fs_node_key_t key;

    err = fs_touch(fs, root_key, "a.txt", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    const size_t file_size = 4096;

    err = fs_resize(fs, key, file_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t *big_buf = da_malloc(file_size);
    TEST_TRUE(big_buf != NULL);

    // 0 out the file.
    mem_set(big_buf, 0, file_size);
    err = fs_write(fs, key, 0, file_size, big_buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    mem_set(big_buf, 1, file_size);
    err = fs_write(fs, key, 1, file_size - 2, big_buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    mem_set(big_buf, 0, file_size);
    err = fs_read(fs, key, 1, file_size - 2, big_buf + 1);

    TEST_EQUAL_UINT(0, big_buf[0]);
    for (size_t i = 1; i < file_size - 1; i++) {
        TEST_EQUAL_UINT(1, big_buf[i]); 
    }
    TEST_EQUAL_UINT(0, big_buf[file_size - 1]);


    // IDK, I guess resize to smaller?

    err = fs_resize(fs, key, file_size / 2);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_write(fs, key, 0, file_size, big_buf);
    TEST_EQUAL_HEX(FOS_INVALID_RANGE,  err);

    da_free(big_buf);

    fs_delete_key(fs, key);

    TEST_SUCCEED();
}

static bool test_fs_rw2(void) {
    // Random test (i.e. my favorite type of test)
    rand_t r = rand(0);

    fernos_error_t err;

    fs_node_key_t key;

    err = fs_touch(fs, root_key, "a.txt", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    size_t file_size = 100;

    err = fs_resize(fs, key, file_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t *buf = da_malloc(file_size);
    TEST_TRUE(buf != NULL);

    mem_set(buf, 0, file_size);

    uint8_t *tx_buf = da_malloc(file_size);
    TEST_TRUE(tx_buf != NULL);

    mem_set(tx_buf, 0, file_size);

    // Set the file as all 0's to start.
    err = fs_write(fs, key, 0, file_size, tx_buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t iter = 0; iter < 1000; iter++) {
        if (next_rand_u1(&r)) { // r/w
            const size_t tx_offset = next_rand_u32(&r) % file_size;
            const size_t tx_amt = (next_rand_u32(&r) % (file_size - tx_offset)) + 1;

            if (next_rand_u1(&r)) { // write
                // Write to our mirror buffer.
                mem_set(buf + tx_offset, iter % 20, tx_amt); 

                // Transfer to the file.
                mem_set(tx_buf + tx_offset, iter % 20, tx_amt); 
                err = fs_write(fs, key, tx_offset, tx_amt, tx_buf + tx_offset);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);
            } else { // read
                // Read from file to the tx buf.

                err = fs_read(fs, key, tx_offset, tx_amt, tx_buf + tx_offset);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);

                // Compare with mirror buffer.
                TEST_TRUE(mem_cmp(buf + tx_offset, tx_buf + tx_offset, tx_amt));
            }
        } else { // resize
            size_t new_file_size = file_size;

            if (next_rand_u1(&r)) { // Stretch. (We will favor stretching)
                new_file_size += 80;
            } else if (file_size >= 100) { // Shrink.
                new_file_size -= 20;
            }

            if (new_file_size != file_size) {
                buf = da_realloc(buf, new_file_size);
                TEST_TRUE(buf != NULL);

                tx_buf = da_realloc(tx_buf, new_file_size);
                TEST_TRUE(buf != NULL);

                err = fs_resize(fs, key, new_file_size);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);

                // Make sure new section added is zero'd out.
                if (new_file_size > file_size) {
                    mem_set(buf + file_size, 0, new_file_size - file_size);
                    mem_set(tx_buf + file_size, 0, new_file_size - file_size);

                    err = fs_write(fs, key, file_size, new_file_size - file_size, tx_buf + file_size);
                    TEST_EQUAL_HEX(FOS_SUCCESS, err);
                }

                file_size = new_file_size;
            }
        }
    }

    // One final check of the full file contents.
    err = fs_read(fs, key, 0, file_size, tx_buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(mem_cmp(buf, tx_buf, file_size));

    // Also let's do an info read cause what the hell.
    fs_node_info_t info;

    err = fs_get_node_info(fs, key, &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(file_size, info.len);

    da_free(tx_buf);
    da_free(buf);

    fs_delete_key(fs, key);

    TEST_SUCCEED();
}


bool test_file_sys(const char *name, file_sys_t *(*gen)(void)) {
    generate_fs = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_fs_touch_and_mkdir);
    RUN_TEST(test_fs_remove);
    RUN_TEST(test_fs_get_child_names);
    RUN_TEST(test_fs_get_node_info);
    RUN_TEST(test_fs_rw0);
    RUN_TEST(test_fs_rw1);
    RUN_TEST(test_fs_rw2);
    return END_SUITE();
}
