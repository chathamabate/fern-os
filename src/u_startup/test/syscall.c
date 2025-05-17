

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

static bool test_complex_fork0(void) {
    fernos_error_t err;

    // First test out some bad reaps.

    err = sc_proc_reap(100, NULL, NULL);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    err = sc_proc_reap(FOS_MAX_PROCS, NULL, NULL);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    sc_signal_allow((1 << FSIG_CHLD));

    // root -> proc0 --*--> proc1
    //                 *--> proc2   
    // 
    // 1) Spawn all processes (proc2 last)
    // 2) proc1 and 2 exit randomly.
    // 3) proc0 is forcefully exited by the FSIG_CHLD signal.
    // 4) Have all processes be reaped from the root.


    proc_id_t cpids[3];

    err = sc_proc_fork(&(cpids[0]));
    if (err != FOS_SUCCESS) {
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    if (cpids[0] == FOS_MAX_PROCS) {

        err = sc_proc_fork(&(cpids[1]));
        if (err != FOS_SUCCESS) {
            sc_term_put_s("Child Failure\n");
            sc_proc_exit(PROC_ES_FAILURE);
        }

        if (cpids[1] == FOS_MAX_PROCS) {
            // inside proc1.

            sc_proc_exit(PROC_ES_SUCCESS);
        }

        err = sc_proc_fork(&(cpids[2]));
        if (err != FOS_SUCCESS) {
            sc_term_put_s("Child Failure\n");
            sc_proc_exit(PROC_ES_FAILURE);
        }

        if (cpids[2] == FOS_MAX_PROCS) {
            // inside proc2
            
            sc_proc_exit(PROC_ES_SUCCESS);
        }

        // inside proc0.
        // Proc0 will be forecfully exited because it does not allow FSIG_CHLD!

        while (1);
    }

    // Inside root procs.

    size_t reaped = 0;

    while (reaped < 3) {
        proc_id_t rcpid;
        proc_exit_status_t rces;

        err = sc_proc_reap(FOS_MAX_PROCS, &rcpid, &rces);
        if (err == FOS_EMPTY) {
            err = sc_signal_wait((1 << FSIG_CHLD), NULL); 
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        } else {
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            reaped++;

            if (rcpid == cpids[0]) {
                TEST_EQUAL_HEX(PROC_ES_SIGNAL, rces);
            }
        } 
    }

    TEST_SUCCEED();
}

bool test_syscall(void) {
    BEGIN_SUITE("Syscall");

    RUN_TEST(test_simple_fork);
    RUN_TEST(test_complex_fork0);
    
    return END_SUITE();
}


