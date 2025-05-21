

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


    // Reaping a non-existent process!
    err = sc_proc_reap(12345, NULL, NULL);
    TEST_EQUAL_HEX(FOS_STATE_MISMATCH, err);

    // Reaping a spceific child process!

    proc_id_t rcpid;
    proc_exit_status_t rces;

    while (1) {
        err = sc_proc_reap(cpid, &rcpid, &rces);
        if (err == FOS_SUCCESS) {
            break;
        }

        err = sc_signal_wait((1 << FSIG_CHLD), NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    TEST_EQUAL_HEX(cpid, rcpid);
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    TEST_SUCCEED();
}

static bool test_simple_signal0(void) {
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

    err = sc_signal_wait((1 << 3), &sid);
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

static bool test_simple_signal1(void) {
    // root --> proc

    fernos_error_t err;
    proc_id_t cpid;

    // Allow all signals to start.
    sc_signal_allow(full_sig_vector());

    err = sc_signal(1234, 1);
    TEST_TRUE(err != FOS_SUCCESS);

    err = sc_signal(FOS_MAX_PROCS, 1);
    TEST_TRUE(err != FOS_SUCCESS);

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        // Send 4,5,6 to the root, then exit.
        for (sig_id_t sid = 4; sid <= 6; sid++) {
            err = sc_signal(FOS_MAX_PROCS, sid);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    sig_id_t rsid0;

    err = sc_signal_wait((1 << 4) | (1 << 6), &rsid0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_TRUE(rsid0 == 4 || rsid0 == 6);

    sig_id_t rsid1;

    err = sc_signal_wait((1 << 4) | (1 << 6), &rsid1);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_TRUE(rsid0 == 4 ? rsid1 == 6 : rsid1 == 4);

    sig_id_t final_rsid;
    err = sc_signal_wait((1 << 5), &final_rsid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_UINT(5, final_rsid);

    err = reap_single(NULL, NULL);
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
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

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

/* Multithreading Tests */

/**
 * Sleep for a little bit, and return the argument multiplied by 10.
 */
static void *example_worker0(void *arg) {
    uint32_t uint_arg = (uint32_t)arg;
    sc_thread_sleep(4);
    return (void *)(uint_arg * 10);
}

static bool test_thread_join0(void) {

    // Maybe like spawn 5 workers, then join them?
    // Make sure their answers all make sense?

    fernos_error_t err;
    join_vector_t jv;

    thread_id_t tids[5];
    for (uint32_t i = 0; i < 5; i++) {
        err = sc_thread_spawn(&(tids[i]), example_worker0, (void *)i);    
        TEST_TRUE(tids[i] < FOS_MAX_THREADS_PER_PROC);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Maybe try joining 3 and then 2 just to jazz things up a bit.

    jv = (1 << tids[0]) | (1 << tids[1]) | (1 << tids[2]);

    thread_id_t joined;
    uint32_t ret_val;

    for (uint32_t i = 0; i < 3; i++) {
        err = sc_thread_join(jv, &joined, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Confirmed the joined tid is tids[0], tids[1], or tids[2]

        uint32_t j;
        for (j = 0; j < 3; j++) {
            if (joined == tids[j]) {
                TEST_EQUAL_UINT(10 * j, ret_val);
                tids[j] = FOS_MAX_THREADS_PER_PROC;
                break;
            }
        }
        TEST_TRUE(j < 3);
    }

    jv = full_join_vector(); // Any threads.
    for (uint32_t i = 0; i < 2; i++) {
        err = sc_thread_join(jv, &joined, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        if (joined == tids[3]) {
            TEST_EQUAL_UINT(10 * 3, ret_val);
            tids[3] = FOS_MAX_THREADS_PER_PROC;
        } else if (joined == tids[4]) {
            TEST_EQUAL_UINT(10 * 4, ret_val);
            tids[4] = FOS_MAX_THREADS_PER_PROC;
        } else {
            TEST_FAIL();
        }
    }
    
    TEST_SUCCEED();
}

/**
 * Spawn 2 worker0 threads, join on them, return their sum.
 * Pass arg to both created threads!
 */
#define WORKER1_SUB_WORKERS (2)
static void *example_worker1(void *arg) {
    fernos_error_t err; 

    thread_id_t tids[WORKER1_SUB_WORKERS];
    for (uint32_t i = 0; i < WORKER1_SUB_WORKERS; i++) {
        err = sc_thread_spawn(&(tids[i]), example_worker0, arg);
        if (err != FOS_SUCCESS) {
            return (void *)(~0U);
        }
    }


    uint32_t sum = 0;
    uint32_t ret_val;

    for (uint32_t i = 0; i < WORKER1_SUB_WORKERS; i++) {
        err = sc_thread_join(1 << tids[i], NULL, (void **)&ret_val);
        if (err != FOS_SUCCESS) {
            return (void *)(~0U);
        }
        sum += ret_val;
    }

    return (void *)sum;
}


static bool test_thread_join1(void) {
    fernos_error_t err;

    thread_id_t tids[4];
    const size_t workers = sizeof(tids) / sizeof(tids[0]);

    join_vector_t jv = empty_join_vector();

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(&(tids[i]), example_worker1, (void *)i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        jv_add_tid(&jv, tids[i]);
    }

    for (uint32_t i = 0; i < workers; i++) {
        thread_id_t joined; 
        uint32_t ret_val;

        err = sc_thread_join(jv, &joined, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        uint32_t j;
        for (j = 0; j < workers; j++) {
            if (tids[j] == joined) {
                TEST_EQUAL_UINT(j * WORKER1_SUB_WORKERS * 10, ret_val);
                tids[j] = FOS_MAX_THREADS_PER_PROC;
                break;
            }
        }

        TEST_TRUE(j < workers);
    }

    TEST_SUCCEED();
}

/*
 * Builtin sync function description for reference:
 *
 * type __sync_val_compare_and_swap (type *ptr, type oldval, type newval, ...)
 * These builtins perform an atomic compare and swap. 
 * That is, if the current value of *ptr is oldval, then write newval into *ptr.
 *
 * The “bool” version returns true if the comparison is successful and newval was written. 
 * The “val” version returns the contents of *ptr before the operation.
 */

/**
 * Here are some states which will make testing a little easier.
 */
static futex_t fut;
static uint32_t number;

/**
 * The argument should be a futex. We expect the futex has value 0.
 */
#define TEST_FUTEX0_WORKER_FLIPS (3)
static void *test_futex0_worker(void *arg) {
    (void)arg;

    fernos_error_t err;
    uint32_t flips = 0;

    while (flips < TEST_FUTEX0_WORKER_FLIPS) {
        uint32_t old_val =  __sync_val_compare_and_swap(&fut, 0, 1);

        if (old_val == 0) { // We acquired number!
            number++;
            sc_thread_sleep(1); // Do "Work"
            number++;

            flips++;
            old_val = __sync_val_compare_and_swap(&fut, 1, 0); // Unlock number.

            err = sc_futex_wake(&fut, false); // Waking 1 should be ok here.
            if (err != FOS_SUCCESS) {
                return (void *)1;
            }

        } else { // other thread is using number.
            err = sc_futex_wait(&fut, 1);
            if (err != FOS_SUCCESS) {
                return (void *)2;
            } 
        }
    }

    return (void *)0;
}

static bool test_futex0(void) {
    fernos_error_t err;

    err = sc_futex_register(NULL);
    TEST_TRUE(err != FOS_SUCCESS);

    fut = 0;
    number = 0;

    err = sc_futex_register(&fut);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = sc_futex_register(&fut);
    TEST_TRUE(err != FOS_SUCCESS);

    const uint32_t workers = 4;

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(NULL, test_futex0_worker, NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    for (uint32_t i = 0; i < workers; i++) {
        uint32_t ret_val;
        err = sc_thread_join(full_join_vector(), NULL, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_UINT(0, ret_val);
    }

    sc_futex_deregister(&fut);

    TEST_EQUAL_UINT(workers * 2 * TEST_FUTEX0_WORKER_FLIPS, number);

    TEST_SUCCEED();
}

static void *test_futex_early_destruct_workcer(void *arg) {
    (void)arg;

    // It will be impossible to determine whether the futex still exists before waiting.
    // If the futex was already deleted, this should return FOS_INVALID_INDEX.
    // If the futex still exists, when it is deleted, the wait call will return FOS_STATE_MISMATCH.
    //
    // We'll check for either.
    fernos_error_t err = sc_futex_wait(&fut, 0);    

    if (err == FOS_INVALID_INDEX || err == FOS_STATE_MISMATCH) {
        return (void *)0;
    }

    return (void *)1;
}

static bool test_futex_early_destruct(void) {
    // We need to create a futex, have multiple threads waiting on the futex,
    // the delete the futex.
    // Waiting threads should be woken up with FOS_STATE_MISMATCH.

    fernos_error_t err;

    fut = 0;

    err = sc_futex_register(&fut);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    const uint32_t workers = 5;

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(NULL, test_futex_early_destruct_workcer, NULL);    
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    sc_thread_sleep(8);
    sc_futex_deregister(&fut);


    for (uint32_t i = 0; i < workers; i++) {
        uint32_t ret_val;

        err = sc_thread_join(full_join_vector(), NULL, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_UINT(0, ret_val);
    }

    TEST_SUCCEED();
}


#define TEST_FORK_AND_THREAD_WORKER_ITERS (5)
static void *test_fork_and_thread_worker(void *arg) {
    (void)arg;

    for (int i = 0; i < TEST_FORK_AND_THREAD_WORKER_ITERS; i++) {
        number++;
        sc_thread_sleep(1);
    }

    return NULL;
}

static bool test_fork_and_thread(void) {
    fernos_error_t err;

    sc_signal_allow((1 << FSIG_CHLD));

    thread_id_t tids[4];
    const uint32_t workers = sizeof(tids) / sizeof(tids[0]);

    number = 0;

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(&(tids[i]), test_fork_and_thread_worker, NULL);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        // The point is that in our child process, none of the spawned workers should be duplicated.
        // We should be able to modify the number freely without any race conditions.
        number = 0;
        test_fork_and_thread_worker(NULL);
        TEST_EQUAL_UINT(TEST_FORK_AND_THREAD_WORKER_ITERS, number);
        sc_proc_exit(PROC_ES_SUCCESS);
    }

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_join(full_join_vector(), NULL, NULL); 
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    proc_exit_status_t rces;
    err = reap_single(NULL, &rces);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    TEST_SUCCEED();
}

/*
 * The below stack pressure tests make sure that the page fault handler is actually doing 
 * what it's supposed to.
 */

static void test_stack_pressure_func(int iters) {
    if (iters == 0) {
        return;
    }

    int i = 10 * 34;
    i++;
    number = i;
    
    test_stack_pressure_func(iters - 1);
}

static void *test_stack_pressure_worker(void *arg) {
    (void)arg;

    test_stack_pressure_func(3000);

    return NULL;
}

static bool test_stack_pressure(void) {
    fernos_error_t err;

    const uint32_t trials = 2;
    const uint32_t workers = 5;

    for (uint32_t t = 0; t < trials; t++) {
        for (uint32_t i = 0; i < workers; i++) {
            err = sc_thread_spawn(NULL, test_stack_pressure_worker, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }

        for (uint32_t i = 0; i < workers; i++) {
            err = sc_thread_join(full_join_vector(), NULL, NULL);
            TEST_EQUAL_HEX(FOS_SUCCESS, err);
        }
    }

    TEST_SUCCEED();
}

static bool test_fatal_stack_pressure(void) {
    fernos_error_t err;

    proc_id_t cpid;

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) {
        // Overflow the stack!
        test_stack_pressure_func(100000000); 
    }

    proc_exit_status_t rces;
    err = reap_single(NULL, &rces);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX(PROC_ES_PF, rces);

    TEST_SUCCEED();
}

bool test_syscall(void) {
    BEGIN_SUITE("Syscall");

    RUN_TEST(test_simple_fork);
    RUN_TEST(test_simple_signal0);
    RUN_TEST(test_simple_signal1);
    RUN_TEST(test_signal_allow_exit);
    RUN_TEST(test_complex_fork0);
    RUN_TEST(test_complex_fork1);
    RUN_TEST(test_complex_fork2);

    // Threading tests

    RUN_TEST(test_thread_join0);
    RUN_TEST(test_thread_join1);
    RUN_TEST(test_futex0);
    RUN_TEST(test_futex_early_destruct);
    RUN_TEST(test_fork_and_thread);

    // Stack pressure tests

    RUN_TEST(test_stack_pressure);
    RUN_TEST(test_fatal_stack_pressure);
    
    return END_SUITE();
}


