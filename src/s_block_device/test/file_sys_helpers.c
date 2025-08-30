
#include "s_block_device/test/file_sys.h"
#include "k_bios_term/term.h"

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

#include "s_block_device/file_sys.h"
#include "s_util/str.h"

static bool test_is_valid_filename(void) {
    const char *VALID_CASES[] = {
        "a.txt",
        ".",
        "..",
        "...",
        "09.ascii",
        "--__.txt",
        "hello-1-2-3.bin"
    };
    const size_t NUM_VALID_CASES = sizeof(VALID_CASES) / sizeof(VALID_CASES[0]);

    for (size_t i = 0; i < NUM_VALID_CASES; i++) {
        TEST_TRUE(is_valid_filename(VALID_CASES[i]));
    }

    const char *INVALID_CASES[] = {
        "",
        "#",
        "c*.txt",
        "&&%%"
    };
    const size_t NUM_INVALID_CASES = sizeof(INVALID_CASES) / sizeof(INVALID_CASES[0]);

    for (size_t i = 0; i < NUM_INVALID_CASES; i++) {
        TEST_FALSE(is_valid_filename(INVALID_CASES[i]));
    }

    // Now test out a name which is too long

    char long_name[FS_MAX_FILENAME_LEN + 2];
    mem_set(long_name, 'a', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = '\0';

    TEST_FALSE(is_valid_filename(long_name));

    TEST_SUCCEED();
}

static bool test_is_valid_path(void) {
    const char *VALID_CASES[] = {
        "/",
        "/hello.txt",
        "./abc.txt",
        "a/./../b/",
        "../a/b/c/hello/1.txt",
        "/Heynow/.txt",
    };
    const size_t NUM_VALID_CASES = sizeof(VALID_CASES) / sizeof(VALID_CASES[0]);

    for (size_t i = 0; i < NUM_VALID_CASES; i++) {
        TEST_TRUE(is_valid_path(VALID_CASES[i]));
    }

    const char *INVALID_CASES[] = {
        "",
        "//",
        "/$.txt",
        "a/b/c//./a.txt"
    };
    const size_t NUM_INVALID_CASES = sizeof(INVALID_CASES) / sizeof(INVALID_CASES[0]);

    for (size_t i = 0; i < NUM_INVALID_CASES; i++) {
        TEST_FALSE(is_valid_path(INVALID_CASES[i]));
    }

    char long_path[FS_MAX_PATH_LEN + 2];

    // Now let's test a path which has a filenmae which is too long.

    long_path[0] = 'a';
    long_path[1] = '/';
    mem_set(long_path + 2, 'a', FS_MAX_FILENAME_LEN + 1);
    long_path[FS_MAX_FILENAME_LEN + 3] = '\0';

    TEST_FALSE(is_valid_path(long_path));

    // Now let's test a path which has valid filename components, but the 
    // overall path is too long.

    for (size_t i = 0; i < sizeof(long_path); i++) {
        long_path[i] = i & 1 ? '.' : '/';
    }

    long_path[FS_MAX_PATH_LEN + 1] = '\0';

    TEST_FALSE(is_valid_path(long_path));

    TEST_SUCCEED();
}

static bool test_next_filename(void) {
    char buf[FS_MAX_FILENAME_LEN + 1];

    const char *path0 = "/abc/cd/a.txt";

    size_t out;

    out = next_filename(path0, buf);
    TEST_EQUAL_UINT(4, out);
    TEST_TRUE(str_eq("abc", buf));

    out += next_filename(path0 + out, buf);
    TEST_EQUAL_UINT(7, out);
    TEST_TRUE(str_eq("cd", buf));

    out += next_filename(path0 + out, buf);
    TEST_EQUAL_UINT(13, out);
    TEST_TRUE(str_eq("a.txt", buf));

    TEST_EQUAL_UINT(0, next_filename(path0 + out, buf));
    TEST_TRUE(str_eq("", buf));

    const char *path1 = "./../";

    out = next_filename(path1, buf);
    TEST_EQUAL_UINT(1, out);
    TEST_TRUE(str_eq(".", buf));

    out += next_filename(path1 + out, buf);
    TEST_EQUAL_UINT(4, out);
    TEST_TRUE(str_eq("..", buf));

    out += next_filename(path1 + out, buf);
    TEST_EQUAL_UINT(5, out);
    TEST_TRUE(str_eq("", buf));

    TEST_SUCCEED();
}

static bool test_separate_path(void) {
    fernos_error_t err;

    struct {
        const char *path;
        const char *exp_dir;
        const char *exp_basename;
    } success_cases[] = {
        {"a", "./", "a"},
        {"/a", "/", "a"},
        {"/a/b/c", "/a/b/", "c"},
        {"./a/.././b", "./a/.././", "b"},
        {"a/b/c", "a/b/", "c"}
    };
    const size_t num_success_cases = sizeof(success_cases) / sizeof(success_cases[0]);

    char dir[FS_MAX_PATH_LEN + 1];
    char basename[FS_MAX_FILENAME_LEN + 1];

    for (size_t i = 0; i < num_success_cases; i++) {
        err = separate_path(success_cases[i].path, dir, basename); 
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(str_eq(success_cases[i].exp_dir, dir)); 
        TEST_TRUE(str_eq(success_cases[i].exp_basename, basename)); 
    }

    const char *bad_cases[] = {
        ".",
        "..",
        ".././",
        "../.",
        "a/",
        "/a/b/c/.."
    };
    const size_t num_bad_cases = sizeof(bad_cases) / sizeof(bad_cases[0]);

    for (size_t i = 0; i < num_bad_cases; i++) {
        err = separate_path(bad_cases[i], dir, basename); 
        TEST_TRUE(err != FOS_SUCCESS);
    }

    TEST_SUCCEED();
}

bool test_file_sys_helpers(void) {
    BEGIN_SUITE("File Sys Helpers");
    RUN_TEST(test_is_valid_filename);
    RUN_TEST(test_is_valid_path);
    RUN_TEST(test_next_filename);
    RUN_TEST(test_separate_path);
    return END_SUITE();
}
