

#include "u_startup/syscall.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static bool test_simple_fork(void) {
    fernos_error_t err;

    proc_id_t cpid;

    sc_signal_allow((1 << FSIG_CHLD));

    err = sc_proc_fork(&cpid);

    if (err != FOS_SUCCESS) {
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    if (cpid == FOS_MAX_PROCS) {
        // We are in the child process!
        sc_proc_exit(PROC_ES_SUCCESS); 
    }

    // Now let's wait on the child process to exit!
    err = sc_signal_wait((1 << FSIG_CHLD), NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    proc_id_t rcpid;
    proc_exit_status_t rces;

    err = sc_proc_reap(FOS_MAX_PROCS, &rcpid, &rces);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    TEST_EQUAL_HEX(cpid, rcpid);
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    TEST_SUCCEED();
}

bool test_syscall(void) {
    BEGIN_SUITE("Syscall");

    RUN_TEST(test_simple_fork);
    
    END_SUITE();

    while (1);

}


