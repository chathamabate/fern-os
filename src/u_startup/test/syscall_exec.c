

#include "u_startup/test/syscall_exec.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

#define TEST_APP "/test_apps/Test"

#define _SYSCALL_EXEC_TEST_DIR "syscall_exec_test_dir"
#define SYSCALL_EXEC_TEST_DIR "/" _SYSCALL_EXEC_TEST_DIR

sig_vector_t old_sv;

static bool pretest(void) {
    TEST_SUCCESS(sc_fs_mkdir(SYSCALL_EXEC_TEST_DIR)); 
    TEST_SUCCESS(sc_fs_set_wd(SYSCALL_EXEC_TEST_DIR));
    
    old_sv = sc_signal_allow(1 << FSIG_CHLD);

    TEST_SUCCEED();
}

static bool posttest(void) {
    sc_signal_allow(old_sv);

    TEST_SUCCESS(sc_fs_set_wd("/"));
    TEST_SUCCESS(sc_fs_remove(SYSCALL_EXEC_TEST_DIR));

    TEST_SUCCEED();
}

static bool t1(void) {
    TEST_SUCCEED();
}

bool test_syscall_exec(void) {
    BEGIN_SUITE("Syscall Exec");
    RUN_TEST(t1);
    return END_SUITE();
}
