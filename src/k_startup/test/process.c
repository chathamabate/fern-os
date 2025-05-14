
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/test/process.h"

#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() lock_up()

#include "s_util/test.h"

static bool pretest(void) {
    TEST_SUCCEED();
}
static bool posttest(void) {
    TEST_SUCCEED();
}

bool test_process(void) {
    BEGIN_SUITE("Process");
    return END_SUITE();
}
