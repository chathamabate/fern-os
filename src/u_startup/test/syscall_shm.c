
#include "u_startup/test/syscall_shm.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static bool test1(void) {
    TEST_SUCCEED();
}

bool test_syscall_shm_sem(void) {
    BEGIN_SUITE("Shared Memory: Semaphores");
    RUN_TEST(test1);
    return END_SUITE();
}
