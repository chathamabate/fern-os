

#include "u_startup/syscall.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static fernos_error_t reap_single(proc_id_t *rcpid, proc_exit_status_t *rces) {
    fernos_error_t err;
    while (1) {
        err = sc_proc_reap(FOS_MAX_PROCS, rcpid, rces);
        if (err != FOS_EMPTY) {
            return err;
        }

        err = sc_signal_wait((1 << FSIG_CHLD), NULL);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }
}

static bool test_simple_fork(void) {
    fernos_error_t err;

    proc_id_t cpid;

    sc_signal_allow((1 << FSIG_CHLD));

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        // We are in the child process!
        sc_proc_exit(PROC_ES_SUCCESS); 
    }

    proc_id_t rcpid;
    proc_exit_status_t rces;

    err = reap_single(&rcpid, &rces);
    TEST_EQUAL_HEX(cpid, rcpid);
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_SUCCEED();
}

static bool test_simple_signal(void) {
    fernos_error_t err;

    sc_signal_allow(full_sig_vector());

    // 1) Parent waits for 3.
    // 2) Child sends 3, waits for 4.
    // 3) Parent receives 3, sends 4.
    // 4) Child receives 4, and exits.
    // 5) Parent waits on FSIG_CHLD, then reaps!

    // Test sending some signals between the parent and the child!

    proc_id_t cpid;

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    sig_id_t sid;

    if (cpid == FOS_MAX_PROCS) {
        // In the child!
        sig_vector_t csv = (1 << 4);

        sc_signal_allow(csv);

        err = sc_signal(FOS_MAX_PROCS, 3);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = sc_signal_wait(csv, &sid);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_UINT(4, sid);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // In the parent process!

    err = sc_signal_wait(full_sig_vector(), &sid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(3, sid);

    err = sc_signal(cpid, 4);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    proc_exit_status_t rces;
    err = reap_single(NULL, &rces);
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_SUCCEED();
}

static bool test_signal_allow_exit(void) {
    // This tests the case where we initially allow all signals, then switch to a more conservative
    // allow vector. A process should always exit if there are pending signals which are no longer
    // allowed.

    fernos_error_t err;

    sc_signal_allow(full_sig_vector()); // allow all signals.

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        sc_signal_allow(full_sig_vector()); 
        // Allow all signals in the child!

        err = sc_signal(FOS_MAX_PROCS, 4);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Wait for the parent to send a 5.
        err = sc_signal_wait((1 << 5), NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        sc_signal_allow(empty_sig_vector());

        while (1);
    }

    err = sc_signal_wait((1 << 4), NULL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_signal(cpid, 4);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // After signaling 5, the child process should disallow all signals. However, at this point
    // there should be a pending 4... Exiting the child process.
    err = sc_signal(cpid, 5);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    
    proc_exit_status_t rces;
    err = reap_single(NULL, &rces);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX(PROC_ES_SIGNAL, rces);
    
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
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpids[0] == FOS_MAX_PROCS) {

        err = sc_proc_fork(&(cpids[1]));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        if (cpids[1] == FOS_MAX_PROCS) {
            // inside proc1.

            sc_proc_exit(PROC_ES_SUCCESS);
        }

        err = sc_proc_fork(&(cpids[2]));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        if (cpids[2] == FOS_MAX_PROCS) {
            // inside proc2
            
            sc_proc_exit(PROC_ES_SUCCESS);
        }

        // inside proc0.
        // Proc0 will be forecfully exited because it does not allow FSIG_CHLD!

        while (1);
    }

    // Inside root procs.

    proc_id_t rcpid;
    proc_exit_status_t rces;
    for (int i = 0; i < 3; i++) {
        err = reap_single(&rcpid, &rces);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        if (rcpid == cpids[0]) {
            TEST_EQUAL_HEX(PROC_ES_SIGNAL, rces);
        }
    }

    TEST_SUCCEED();
}

static bool test_complex_fork1(void) {
    // root -> proc0 -> proc1 -> proc2
    // proc1 signals proc2, they all exit due to a FSIG_CHLD chain.
    // root reaps 3 processes.

    fernos_error_t err;

    sig_vector_t tmp = (1 << 3) | (1 << 4) | (1 << FSIG_CHLD);

    sc_signal_allow(tmp);
    sig_vector_t old = sc_signal_allow(full_sig_vector());

    // Confirm the swap mechanism works correctly.
    TEST_EQUAL_HEX(tmp, old);

    proc_id_t cpid;

    int i;

    for (i = 0; i < 3; i++) {
        err = sc_proc_fork(&cpid);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        if (cpid != FOS_MAX_PROCS) {
            break;
        }
    }

    if (i == 2) {
        // Kill child 3 with an arbitrary signal.
        err = sc_signal(cpid, 1 << 4);
        if (err != FOS_SUCCESS) {
            sc_term_put_s("Signal Error\n");
        }
        while (1);
    } else if (i == 0) {
        for (int i = 0; i < 3; i++) {
            err = reap_single(NULL, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }

        TEST_SUCCEED();
    } else {
        while (1);
    }
}

static bool test_complex_fork2(void) {
    // root -> proc  --*--> proc0
    //                 *--> proc1
    //                 *--> proc2

    // proc1 signals proc0 to die.
    // proc reaps proc0, signals proc1 and proc2, then exits.
    // root reaps proc1, proc2, and proc.

    sc_signal_allow(full_sig_vector());

    fernos_error_t err;
    proc_id_t cpid;

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        sc_signal_allow(1 << FSIG_CHLD);

        proc_id_t cpids[3];

        int i;
        for (i = 0; i < 3; i++) {
            err = sc_proc_fork(&(cpids[i]));
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            if (cpids[i] == FOS_MAX_PROCS) {
                break;
            }
        }

        if (i == 3) {
            // proc.
            sc_signal_allow(1 << FSIG_CHLD);

            proc_id_t rcpid;
            err = reap_single(&rcpid, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
            TEST_EQUAL_HEX(cpids[0], rcpid);

            err = sc_signal(cpids[1], 4);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            err = sc_signal(cpids[2], 4);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            sc_proc_exit(PROC_ES_SUCCESS);
        } 

        if (cpids[0] == FOS_MAX_PROCS) {
            // proc0
            while (1);
        }

        if (cpids[1] == FOS_MAX_PROCS) {
            // proc1
            err = sc_signal(cpids[0], 4);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);

            while (1);
        }

        if (cpids[2] == FOS_MAX_PROCS) {
            // proc2
            while (1);
        }
    }

    for (int i = 0; i < 3; i++) {
        // reap proc, proc1, and proc2
        err = reap_single(NULL, NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    TEST_SUCCEED();
}

bool test_syscall(void) {
    BEGIN_SUITE("Syscall");

    RUN_TEST(test_simple_fork);
    RUN_TEST(test_simple_signal);
    RUN_TEST(test_signal_allow_exit);
    RUN_TEST(test_complex_fork0);
    RUN_TEST(test_complex_fork1);
    RUN_TEST(test_complex_fork2);
    
    return END_SUITE();
}


