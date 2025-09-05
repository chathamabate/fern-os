
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "s_util/constraints.h"
#include "s_util/str.h"
#include <stdarg.h>
#include "s_data/map.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

/*
 * NOTE: These tests are not meant to rigorosly test reading a writing to files/building
 * complex file trees. These cases are already covered by the file system tests inside the kernel.
 *
 * These tests make sure that processes and threads behave as expected when reading/writing to
 * file concurrently.
 */

/**
 * Each test will start in an empty instance of this directory. 
 * At the end of each test, this directory will be deleted.
 */
#define _TEMP_TEST_DIR_PATH "syscall_fs_test"
#define TEMP_TEST_DIR_PATH "/" _TEMP_TEST_DIR_PATH 

sig_vector_t old_sv;

static bool pretest(void) {
    fernos_error_t err;

    // Since we will be working with child processes, we will allow the 
    // child signal in root.
    old_sv = sc_signal_allow(1 << FSIG_CHLD);

    err = sc_fs_mkdir(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_set_wd(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool posttest(void) {
    fernos_error_t err;

    err = sc_fs_set_wd("/");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_remove(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_flush(FOS_MAX_FILE_HANDLES_PER_PROC);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sc_signal_allow(old_sv);

    TEST_SUCCEED();
}

static bool test_simple_rw(void) {
    fernos_error_t err;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t fh;

    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    const char *msg = "Hello FS";
    const size_t msg_size = str_len(msg) + 1;

    err = sc_fs_write_full(fh, msg, msg_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    fs_node_info_t info;
    err = sc_fs_get_info("./a.txt", &info);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(msg_size, info.len);

    err = sc_fs_seek(fh, 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    char rx_buf[50];
    err = sc_fs_read_full(fh, rx_buf, msg_size);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    
    TEST_TRUE(str_eq(msg, rx_buf));

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    sc_fs_close(fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_multithread_rw_worker(void *arg) {
    (void)arg;

    fernos_error_t err;

    file_handle_t fh;

    err = sc_fs_open("./b.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t rx_buf[10];

    for (size_t i = 0; i < 10; i++) {
        err = sc_fs_read_full(fh, rx_buf, sizeof(rx_buf));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (size_t j = 0; j < sizeof(rx_buf); j++) {
            TEST_EQUAL_UINT(i, rx_buf[j]);
        }
    }

    sc_fs_close(fh);

    return NULL;
}

static bool test_multithread_rw(void) {
    fernos_error_t err;

    err = sc_fs_touch("./b.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_thread_spawn(NULL, test_multithread_rw_worker, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t fh;

    err = sc_fs_open("./b.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t tx_buf[10];

    for (size_t i = 0; i < 10; i++) {
        mem_set(tx_buf, (uint8_t)i, sizeof(tx_buf));

        err = sc_fs_write_full(fh, tx_buf, sizeof(tx_buf));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        sc_thread_sleep(1);
    }

    sc_fs_close(fh);

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sc_fs_remove("./b.txt");
    
    TEST_SUCCEED();
}

static bool test_multiprocess_rw(void) {
    fernos_error_t err;

    // Pretty simple test, write from parent, read from child.

    // Trying an absolute path because what the hell.
    err = sc_fs_touch(TEMP_TEST_DIR_PATH "/a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t fh;

    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    uint8_t buf[100];

    mem_set(buf, 5, sizeof(buf));

    // Let's start by advancing the position of the file handle to confirm
    // the file handle copy works as expected!

    err = sc_fs_write_full(fh, buf, sizeof(buf));
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) { // Child process!
        for (size_t i = 0; i < 10; i++) {
            err = sc_fs_read_full(fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            for (size_t j = 0; j < sizeof(buf); j++) {
                TEST_EQUAL_UINT(i, buf[j]);
            }
        }

        // We will exit WITHOUT closing our file handle. This is to test that
        // the file handle is closed automatically during a reap.
        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // Parent process!
    for (size_t i = 0; i < 10; i++) {
        mem_set(buf, i, sizeof(buf));

        err = sc_fs_write_full(fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        sc_thread_sleep(1);
    }

    err = sc_signal_wait(1 <<  FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sc_fs_close(fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_multithread_and_process_rw_worker(void *arg) {
    (void)arg;

    fernos_error_t err;

    file_handle_t b_fh;
    err = sc_fs_open("./b.txt", &b_fh);

    uint8_t buf[100];

    for (size_t i = 0; i < 10; i++) {
        err = sc_fs_read_full(b_fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (size_t j = 0; j < sizeof(buf); j++) {
            TEST_EQUAL_UINT(i + 1, buf[j]);
        }
    }

    sc_fs_close(b_fh);

    return NULL;
}

static bool test_multithread_and_process_rw(void) {
    //  (root proc)           (child proc)
    //     worker0  --> "a.txt"  ---
    //                             |
    //     worker1  <-- "b.txt"  <--

    fernos_error_t err;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_touch("./b.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t a_fh;

    uint8_t buf[100];

    err = sc_fs_open("./a.txt", &a_fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) { // Child process
        file_handle_t b_fh;

        err = sc_fs_open("./b.txt", &b_fh);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (size_t i = 0; i < 10; i++) {
            err = sc_fs_read_full(a_fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            for (size_t j = 0; j < sizeof(buf); j++) {
                buf[j]++;
            }

            err = sc_fs_write_full(b_fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }

        sc_fs_close(a_fh);
        sc_fs_close(b_fh);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // Parent process.
    
    err = sc_thread_spawn(NULL, test_multithread_and_process_rw_worker, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0; i < 10; i++) {
        mem_set(buf, i, sizeof(buf));

        err = sc_fs_write_full(a_fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    err = sc_signal_wait(1 << FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // Wait for middle man process to exit before joining on worker thread.

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sc_fs_close(a_fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_remove("./b.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_early_close_worker(void *arg) {
    file_handle_t fh = (file_handle_t)arg;

    uint32_t i;

    fernos_error_t err;

    err = sc_fs_read_full(fh, &i, sizeof(i));
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Trying to read from a handle which doesn't exist!
    err = sc_fs_read_full(fh, &i, sizeof(i));
    TEST_TRUE(err != FOS_SUCCESS);

    return NULL;
}

static bool test_early_close(void) {
    fernos_error_t err;

    // Here we test that closing all handles before a read is done causes an
    // early wakeup! (With FOS_STATE_MISMATCH as the return)

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t fh;
    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_thread_spawn(NULL, test_early_close_worker, (void *)fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // Ok, this isn't really the best made test, we are assuming that by the time
    // time we wake up, the worker will have started to wait on data over `fh`.
    // This might not always be gauranteed. However, the read call will ALWAYS fail.
    sc_thread_sleep(5);

    // Close the very handle the worker is reading from :(
    sc_fs_close(fh);

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_many_handles(void) {
    // Test pushing the handle limit.

    fernos_error_t err;

    file_handle_t handles[FOS_MAX_FILE_HANDLES_PER_PROC];

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0; i < FOS_MAX_FILE_HANDLES_PER_PROC; i++) {
        err = sc_fs_open("./a.txt", handles + i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    proc_id_t cpid;

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // We are going to do this in BOTH PROCESSES!
    file_handle_t bad_handle;
    err = sc_fs_open("./a.txt", &bad_handle);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    uint8_t buf[FOS_MAX_FILE_HANDLES_PER_PROC];

    if (cpid == FOS_MAX_PROCS) {
        // Alright this is going to be a bit tricky!
        // Remember, that all handles have their OWN positions!!!
        for (size_t i = 0; i < FOS_MAX_FILE_HANDLES_PER_PROC; i++) {
            mem_set(buf, i, i + 1);

            err = sc_fs_write_full(handles[i], buf, i + 1);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            sc_fs_close(handles[i]);
        }
        
        // This confirms that all of our handles started at position 0.

        fs_node_info_t info;
        err = sc_fs_get_info("./a.txt", &info);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_EQUAL_UINT(FOS_MAX_FILE_HANDLES_PER_PROC, info.len);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    TEST_TRUE(FOS_MAX_FILE_HANDLES_PER_PROC > 1);

    err = sc_fs_read_full(handles[0], buf, FOS_MAX_FILE_HANDLES_PER_PROC);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // Since we read the full buffer each time, after the second read, we can gaurantee
    // we are reading the entire file in its final state.
    
    err = sc_fs_read_full(handles[1], buf, FOS_MAX_FILE_HANDLES_PER_PROC);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (size_t i = 0 ; i < sizeof(buf); i++) {
        TEST_EQUAL_UINT(FOS_MAX_FILE_HANDLES_PER_PROC - 1, buf[i]);
    }

    for (size_t i = 0; i < FOS_MAX_FILE_HANDLES_PER_PROC; i++) {
        sc_fs_close(handles[i]);
    }

    // There should be space at the very end!
    err = sc_fs_open("./a.txt", handles + 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sc_fs_close(handles[0]);

    err = sc_signal_wait(1 << FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_dir_functions(void) {
    // Test functions like mkdir, set_wd, and get child name.
    // Pretty simple test tbh.

    fernos_error_t err;

    err = sc_fs_mkdir("./other_dir");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    char name_bufs[2][FS_MAX_FILENAME_LEN + 1];
    
    err = sc_fs_get_child_name(".", 0, name_bufs[0]);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_get_child_name(".", 1, name_bufs[1]);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_TRUE(!str_eq(name_bufs[0], name_bufs[1]));
    for (size_t i = 0; i < 2; i++) {
        TEST_TRUE(str_eq("a.txt", name_bufs[i]) || str_eq("other_dir", name_bufs[i]));
    }

    err = sc_fs_get_child_name(".", 2, name_bufs[0]);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_remove("a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_set_wd("..");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_remove(_TEMP_TEST_DIR_PATH "/other_dir");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_fs_set_wd(_TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_bad_fs_calls(void) {
    fernos_error_t err;

    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // Can't set working directory to a file.
    err = sc_fs_set_wd("a.txt");
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Can't set working directory to a directory which doesn't exist.
    err = sc_fs_set_wd("./fake_dir");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // Trying to create a file which doesn't exist.
    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    // This path doesn't exist.
    err = sc_fs_touch("b/a.txt");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // Same outcomes expected for mkdir.
    err = sc_fs_mkdir("a.txt");
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    err = sc_fs_mkdir("b/a.txt");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // Can't delete a non-empty directory.
    err = sc_fs_remove(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_IN_USE, err);

    // Can't delete a file which doesn't exist.
    err = sc_fs_remove("c.txt");
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    // OK Now for get info/get childname
    fs_node_info_t info;

    err = sc_fs_get_info("b.txt", &info);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    char child_name[FS_MAX_FILENAME_LEN + 1];
    err = sc_fs_get_child_name("b", 0, child_name);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_get_child_name("a.txt", 0, child_name);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Open and close.

    file_handle_t fh;

    err = sc_fs_open("b", &fh);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_open(".", &fh);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Some random invalid file handle shouldn't cause issues.
    sc_fs_close((file_handle_t)238438);

    // Seek/read/write
    
    err = sc_fs_seek((file_handle_t)123, 3);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    size_t txed;
    uint32_t dummy;

    err = sc_fs_write((file_handle_t)3432, &dummy, sizeof(dummy), &txed);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_read((file_handle_t)3432, &dummy, sizeof(dummy), &txed);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_flush((file_handle_t)3432);
    TEST_EQUAL_HEX(FOS_INVALID_INDEX, err);

    err = sc_fs_remove("a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}

bool test_syscall_fs(void) {
    BEGIN_SUITE("FS Syscalls");
    RUN_TEST(test_simple_rw);
    RUN_TEST(test_multithread_rw);
    RUN_TEST(test_multiprocess_rw);
    RUN_TEST(test_multithread_and_process_rw);
    RUN_TEST(test_early_close);
    RUN_TEST(test_many_handles);
    RUN_TEST(test_dir_functions);
    RUN_TEST(test_bad_fs_calls);
    return END_SUITE();
}
