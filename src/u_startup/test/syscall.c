

#include "u_startup/syscall.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

bool test_syscall(void) {
    BEGIN_SUITE("Syscall");
    
    return END_SUITE();
}


