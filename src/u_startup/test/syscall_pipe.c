
#include "u_startup/syscall.h"
#include "u_startup/syscall_pipe.h"
#include "u_startup/test/syscall_pipe.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

static size_t old_user_al;
static bool pretest(void) {
    TEST_TRUE(get_default_allocator() != NULL);
    old_user_al = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}

static bool posttest(void) {
    size_t new_user_al = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(old_user_al, new_user_al);
    TEST_SUCCEED();
}

static bool test1(void) {
    TEST_SUCCEED();
}

bool test_syscall_pipe(void) {
    BEGIN_SUITE("Syscall Pipe");
    return END_SUITE();
}
