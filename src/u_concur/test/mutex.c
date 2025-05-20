
#include "u_startup/syscall.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

bool test_mutex(void) {
    BEGIN_SUITE("Mutex");

    return END_SUITE();
}
