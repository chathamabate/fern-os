
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

static bool try_write_full(file_handle_t fh, const void *src, size_t len) {
    fernos_error_t err;

    size_t written = 0;
    while (written < len) {
        size_t act_written;

        err = sc_fs_write(fh, (const uint8_t *)src + written, len - written, &act_written);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        written += act_written;
    }

    TEST_SUCCEED();
}

static bool try_read_full(file_handle_t fh, void *dest, 

static bool test_simple_rw(void) {
    fernos_error_t err;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    file_handle_t fh;

    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    const char *msg = "Hello FS";

    TEST_TRUE(try_write_full(fh, msg, str_len(msg + 1)));

    err = sc_fs_seek(fh, 0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_SUCCEED();
}


bool test_syscall_fs(void) {
    BEGIN_SUITE("FS Syscalls");
    RUN_TEST(test_simple_rw);
    return END_SUITE();
}
