
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static bool dummy(void) {
    TEST_SUCCEED();
}

bool test_syscall_fs(void) {
    BEGIN_SUITE("FS Syscalls");
    RUN_TEST(dummy);
    return END_SUITE();
}
