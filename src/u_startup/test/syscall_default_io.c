
#include "u_startup/test/syscall_default_io.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_fs.h"

#include "s_bridge/shared_defs.h"


static handle_t results_cd;

#define LOGF_METHOD(...) sc_handle_write_fmt_s(results_cd, __VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

static bool pretest(void) {
    TEST_SUCCEED();
} 

static bool posttest(void) {
    TEST_SUCCEED();
}

bool test_syscall_default_io(handle_t cd) {
    results_cd = cd;

    BEGIN_SUITE("Syscall Default IO");

    return END_SUITE();
}
