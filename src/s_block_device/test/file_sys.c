
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

/*
 * Honestly, this test suite could be better. A file system is such a big thing,
 * comprehensive testing is difficult.
 */

static bool test_fs_touch_and_mkdir(void) {
    fernos_error_t err;

    // First, let's let's just try these unique cases.
    // Know that "." and ".." ARE valid filenames technically,
    // however, you are not allowed to created files/subdirectories with their name!
    err = fs_touch(fs, root_key, ".", NULL);
    TEST_EQUAL_HEX(FOS_BAD_ARGS, err);
    err = fs_touch(fs, root_key, "..", NULL);
    TEST_EQUAL_HEX(FOS_BAD_ARGS, err);

    err = fs_mkdir(fs, root_key, ".", NULL);
    TEST_EQUAL_HEX(FOS_BAD_ARGS, err);
    err = fs_mkdir(fs, root_key, "..", NULL);
    TEST_EQUAL_HEX(FOS_BAD_ARGS, err);

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

    // While "." and ".." likely exist and are valid in the given directory,
    // they should never be removeable!
    err = fs_remove(fs, subdir_key, ".");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = fs_remove(fs, subdir_key, "..");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

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

static bool test_fs_hash_equate_and_copy(void) {
    fernos_error_t err;

    hasher_ft hash_func = fs_get_key_hasher(fs);
    TEST_TRUE(hash_func != NULL);

    equator_ft equate_func = fs_get_key_equator(fs);
    TEST_TRUE(equate_func != NULL);

    fs_node_key_t keys[4];

    err = fs_touch(fs, root_key, "a.txt", keys + 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_new_key(fs, root_key, "a.txt", keys + 1);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch(fs, root_key, "b.txt", keys + 2);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    keys[3] = fs_new_key_copy(fs, keys[2]);
    TEST_TRUE(keys[3] != NULL);

    TEST_EQUAL_UINT(hash_func(keys + 0), hash_func(keys + 1));
    TEST_TRUE(equate_func(keys + 0, keys + 1));

    TEST_FALSE(equate_func(keys + 1, keys + 2));

    TEST_EQUAL_UINT(hash_func(keys + 2), hash_func(keys + 3));
    TEST_TRUE(equate_func(keys + 2, keys + 3));

    for (size_t i = 0; i < 4; i++) {
        fs_delete_key(fs, keys[i]);
    }

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

    err = fs_write(fs, key, 0, (file_size / 2) + 1, big_buf);
    TEST_EQUAL_HEX(FOS_INVALID_RANGE,  err);

    err = fs_write(fs, key, 1, (file_size / 2), big_buf);
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

    fs_node_key_t key_copy = fs_new_key_copy(fs, key);
    TEST_TRUE(key_copy != NULL);

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

                // Reading from the key copy should not result in any issues.
                // I just threw this in here to get more coverage on the copy key code.
                err = fs_read(fs, key_copy, tx_offset, tx_amt, tx_buf + tx_offset);
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

                // lastly, just as a sanity check, let's confirm that resizing always updates
                // the node info correctly!

                fs_node_info_t info;
                err = fs_get_node_info(fs, key, &info);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);

                TEST_EQUAL_UINT(new_file_size, info.len);

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

    fs_delete_key(fs, key_copy);
    fs_delete_key(fs, key);

    TEST_SUCCEED();
}

static bool test_fs_random_file_tree(void) {
    rand_t r = rand(0);

    fernos_error_t err;

    fs_node_key_t dir_keys[50];
    const size_t dir_keys_cap = sizeof(dir_keys) / sizeof(dir_keys[0]);

    err = fs_new_key(fs, NULL, "/", &(dir_keys[0]));
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    size_t dir_keys_len = 1;

    // Here we make some random directories.

    while (dir_keys_len < dir_keys_cap) {
        const fs_node_key_t dir_key = dir_keys[next_rand_u32(&r) % dir_keys_len];

        const char new_dir_name[2] = {
            'a' + (next_rand_u8(&r) % 26), '\0'
        };

        err = fs_mkdir(fs, dir_key, new_dir_name, &(dir_keys[dir_keys_len]));
        TEST_TRUE(err == FOS_SUCCESS || err == FOS_IN_USE);

        if (err == FOS_SUCCESS) {
            dir_keys_len++;
        }
    }

    // Next let's make some random files.

    uint8_t rw_buf[1024];
    const size_t rw_buf_size = sizeof(rw_buf) / sizeof(rw_buf[0]);

    hasher_ft hash_func = fs_get_key_hasher(fs);
    TEST_TRUE(hash_func != NULL);

    const size_t files_to_create = 100;
    size_t num_files = 0;

    while (num_files < files_to_create) {
        // Here we know the dirs_keys array is full.
        const fs_node_key_t dir_key = dir_keys[next_rand_u32(&r) % dir_keys_cap];

        const char new_file_name[6] = {
            'A' + (next_rand_u8(&r) % 26), '.', 't', 'x', 't', '\0'
        };

        fs_node_key_t file_key;

        err = fs_touch(fs, dir_key, new_file_name, &file_key);
        TEST_TRUE(err == FOS_SUCCESS || err == FOS_IN_USE);

        if (err == FOS_SUCCESS) {
            const uint32_t hash = hash_func(&file_key);
            const size_t new_size = (hash % rw_buf_size);

            err = fs_resize(fs, file_key, new_size);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            mem_set(rw_buf, (uint8_t)hash, new_size);
            err = fs_write(fs, file_key, 0, new_size, rw_buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_delete_key(fs, file_key);
            num_files++;
        }
    }

    for (size_t i = 0; i < dir_keys_cap; i++) {
        fs_delete_key(fs, dir_keys[i]);
    }

    // Now we are going to go through each file randomly, confirming each file's contents.
    // (NOTE: This will most definitely not hit every created file, but that's ok)
    for (size_t trial = 0; trial < files_to_create; trial++) {
        fs_node_key_t iter;

        err = fs_new_key(fs, NULL, "/", &iter);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        while (true) {
            fs_node_info_t info;

            err = fs_get_node_info(fs, iter, &info); 
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            if (!(info.is_dir)) { // Data file.
                const uint32_t hash = hash_func(&iter); 
                const uint32_t exp_size = hash % rw_buf_size;

                err = fs_read(fs, iter, 0, exp_size, rw_buf);
                TEST_EQUAL_HEX(FOS_SUCCESS, err);

                // Confirm contents.
                for (size_t i = 0; i < exp_size; i++) {
                    TEST_EQUAL_UINT((uint8_t)hash, rw_buf[i]);
                }

                // Success Exit this trial!
                break;
            } 

            // Sub directory case.

            if (info.len == 0) { // We reached an empty subdirectory :(
                break;
            }

            // Non-empty subdirectory!
            
            char name_buf[1][FS_MAX_FILENAME_LEN + 1];

            err = fs_get_child_names(fs, iter, next_rand_u32(&r) % info.len, 1, name_buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_node_key_t next_iter;
            err = fs_new_key(fs, iter, name_buf[0], &next_iter);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_delete_key(fs, iter);
            iter = next_iter;
        }

        fs_delete_key(fs, iter);
    }

    TEST_SUCCEED();
}

static bool test_fs_zero_resize(void) {
    // Resizing to size 0 should succeed and NOT delete the file.
    // That is what this tests.
    
    fernos_error_t err;

    fs_node_key_t child_key;
    err = fs_touch(fs, root_key, "a.txt", &child_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_resize(fs, child_key, 100);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_resize(fs, child_key, 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_delete_key(fs, child_key);

    err = fs_new_key(fs, root_key, "a.txt", &child_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_delete_key(fs, child_key);

    TEST_SUCCEED();
}

static bool test_fs_exhaust(void) {
    // Try salvaging the file system even after using up all disk space.

    fernos_error_t err;

    uint8_t buf[100];
    const size_t buf_size = sizeof(buf);

    fs_node_key_t og_file_key;
    err = fs_touch(fs, root_key, "a.txt", &og_file_key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_resize(fs, og_file_key, buf_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    mem_set(buf, 1, buf_size);
    err = fs_write(fs, og_file_key, 0, buf_size, buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    char name_buf[FS_MAX_FILENAME_LEN + 1];

    uint32_t created = 0;
    while (true) {
        fs_node_key_t key;
        str_fmt(name_buf, "%u", created);

        err = fs_touch(fs, root_key, name_buf, &key);
        if (err == FOS_NO_SPACE) {
            break;
        }
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Successfully created a file!
        created++;

        err = fs_resize(fs, key, 2048);
        fs_delete_key(fs, key);

        if (err == FOS_NO_SPACE) {
            break;
        }
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    err = fs_resize(fs, og_file_key, buf_size + 2048);
    TEST_EQUAL_HEX(FOS_NO_SPACE, err);

    // Confirm reading works even with no space!

    mem_set(buf, 0, buf_size);
    err = fs_read(fs, og_file_key, 0, buf_size, buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0; i < buf_size; i++) {
        TEST_EQUAL_UINT(1, buf[i]);
    }

    // Now delete all created files.
    for (size_t i = 0; i < created; i++) {
        str_fmt(name_buf, "%u", i);
        err = fs_remove(fs, root_key, name_buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Should be able to resize fine here after clearing things up.
    err = fs_resize(fs, og_file_key, buf_size + 2048);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    
    fs_delete_key(fs, og_file_key);

    TEST_SUCCEED();
}

static bool test_fs_manual_steps(void) {
    // This test will try out some moves I write up myself (instead of being
    // random like the above tests)

    typedef enum {
        STEP_TOUCH, // (void)
        STEP_MKDIR, // (void)
        STEP_REMOVE,// (void)
        STEP_READ,  // (offset, amt, expected_val)
        STEP_WRITE, // (offset, amt, value_to_write)
        STEP_RESIZE // (new_size)
    } step_type_t;

    typedef struct {
        step_type_t type;

        const char *dir_path;
        const char *base_name;

        uint32_t args[3];
    } step_t;

    static const step_t steps[] = {
        {STEP_TOUCH, "/", "a.bin", {0}},
        {STEP_MKDIR, ".", "b", {0}},
        {STEP_TOUCH, "./b", "b.bin", {0}},
        {STEP_RESIZE, "./b", "b.bin", {100}},
        {STEP_WRITE, "/b", "b.bin", {0, 20, 5}},
        {STEP_WRITE, "/b", "b.bin", {20, 80, 4}},
        {STEP_RESIZE, "/", "a.bin", {200}},
        {STEP_TOUCH, "/", "c.bin", {0}},
        {STEP_READ, "./b", "b.bin", {0, 20, 5}},
        {STEP_WRITE, "./", "a.bin", {0, 200, 1}},
        {STEP_READ, "./b", "b.bin", {20, 80, 4}},
        {STEP_REMOVE, "./b/", "b.bin", {0}},
        {STEP_RESIZE, "/", "c.bin", {1000}},
        {STEP_READ, "./", "a.bin", {0, 200, 1}}

        // Add more if you'd like...
    };
    const size_t num_steps = sizeof(steps) / sizeof(steps[0]);

    fernos_error_t err;

    fs_node_key_t dir_key;
    fs_node_key_t child_key;

    uint8_t buf[1024];
    const size_t buf_size = sizeof(buf);

    for (size_t step_i = 0; step_i < num_steps; step_i++) {
        const step_t step = steps[step_i];

        err = fs_new_key(fs, root_key, step.dir_path, &dir_key);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        switch (step.type) {
        case STEP_TOUCH:
            err = fs_touch(fs, dir_key, step.base_name, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            break;
        case STEP_MKDIR:
            err = fs_mkdir(fs, dir_key, step.base_name, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            break;
        case STEP_REMOVE:
            err = fs_remove(fs, dir_key, step.base_name);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            break;
        case STEP_READ:
            TEST_TRUE(step.args[1] <= buf_size);

            // Fill the buffer with an unexpected value first.
            mem_set(buf, (uint8_t)(step.args[2]) + 1, step.args[1]);
             
            err = fs_new_key(fs, dir_key, step.base_name, &child_key);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            err = fs_read(fs, child_key, step.args[0], step.args[1], buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_delete_key(fs, child_key);

            for (size_t i = 0; i < step.args[1]; i++) {
                TEST_EQUAL_UINT(step.args[2], buf[i]);
            }

            break;
        case STEP_WRITE:
            TEST_TRUE(step.args[1] <= buf_size);
            mem_set(buf, (uint8_t)(step.args[2]), step.args[1]);

            err = fs_new_key(fs, dir_key, step.base_name, &child_key);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            err = fs_write(fs, child_key, step.args[0], step.args[1], buf);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_delete_key(fs, child_key);

            break;
        case STEP_RESIZE:
            err = fs_new_key(fs, dir_key, step.base_name, &child_key);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            err = fs_resize(fs, child_key, step.args[0]);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            fs_delete_key(fs, child_key);
            break;
        }

        fs_delete_key(fs, dir_key);
    }

    TEST_SUCCEED();
}

static bool test_fs_path_helpers(void) {
    // Confirm the default path wrapper functions work as expected.
    // This test is pretty lazy tbh. IDK, the wrapper are pretty simple and all the same.

    fernos_error_t err;

    err = fs_touch_path(fs, NULL, "/a.txt", NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch_path(fs, root_key, "./b.txt", NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_node_key_t key;

    err = fs_mkdir_path(fs, root_key, "./c", &key);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch_path(fs, root_key, "./c/b.txt", NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = fs_touch_path(fs, root_key, "./d/b.txt", NULL);
    TEST_TRUE(FOS_SUCCESS != err);

    err = fs_remove_path(fs, key, "b.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

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
    RUN_TEST(test_fs_hash_equate_and_copy);
    RUN_TEST(test_fs_rw0);
    RUN_TEST(test_fs_rw1);
    RUN_TEST(test_fs_rw2);
    RUN_TEST(test_fs_random_file_tree);
    RUN_TEST(test_fs_zero_resize);
    RUN_TEST(test_fs_exhaust);
    RUN_TEST(test_fs_manual_steps);
    RUN_TEST(test_fs_path_helpers);
    return END_SUITE();
}
