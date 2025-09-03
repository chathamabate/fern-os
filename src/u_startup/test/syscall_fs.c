
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "s_util/constraints.h"
#include "s_util/str.h"

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
#define TEMP_TEST_DIR_PATH "/syscall_fs_test"

static bool pretest(void) {
    fernos_error_t err;

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


bool test_syscall_fs(void) {
    BEGIN_SUITE("FS Syscalls");
    RUN_TEST(test_simple_rw);
    RUN_TEST(test_multithread_rw);
    return END_SUITE();
}
