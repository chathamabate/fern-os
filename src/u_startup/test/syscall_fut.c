
#include "u_startup/test/syscall_fut.h"
#include "u_startup/syscall_fut.h"
#include "u_startup/syscall.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

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

            err = sc_fut_wake(&fut, false); // Waking 1 should be ok here.
            if (err != FOS_E_SUCCESS) {
                return (void *)1;
            }

        } else { // other thread is using number.
            err = sc_fut_wait(&fut, 1);
            if (err != FOS_E_SUCCESS) {
                return (void *)2;
            } 
        }
    }

    return (void *)0;
}

static bool test_futex0(void) {
    fernos_error_t err;

    err = sc_fut_register(NULL);
    TEST_TRUE(err != FOS_E_SUCCESS);

    fut = 0;
    number = 0;

    err = sc_fut_register(&fut);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fut_register(&fut);
    TEST_TRUE(err != FOS_E_SUCCESS);

    const uint32_t workers = 4;

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(NULL, test_futex0_worker, NULL);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    for (uint32_t i = 0; i < workers; i++) {
        uint32_t ret_val;
        err = sc_thread_join(full_join_vector(), NULL, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_UINT(0, ret_val);
    }

    sc_fut_deregister(&fut);

    TEST_EQUAL_UINT(workers * 2 * TEST_FUTEX0_WORKER_FLIPS, number);

    TEST_SUCCEED();
}

static void *test_futex_early_destruct_workcer(void *arg) {
    (void)arg;

    // It will be impossible to determine whether the futex still exists before waiting.
    // If the futex was already deleted, this should return FOS_E_INVALID_INDEX.
    // If the futex still exists, when it is deleted, the wait call will return FOS_E_STATE_MISMATCH.
    //
    // We'll check for either.
    fernos_error_t err = sc_fut_wait(&fut, 0);    

    if (err == FOS_E_INVALID_INDEX || err == FOS_E_STATE_MISMATCH) {
        return (void *)0;
    }

    return (void *)1;
}

static bool test_futex_early_destruct(void) {
    // We need to create a futex, have multiple threads waiting on the futex,
    // the delete the futex.
    // Waiting threads should be woken up with FOS_E_STATE_MISMATCH.

    fernos_error_t err;

    fut = 0;

    err = sc_fut_register(&fut);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    const uint32_t workers = 5;

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(NULL, test_futex_early_destruct_workcer, NULL);    
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    sc_thread_sleep(8);
    sc_fut_deregister(&fut);


    for (uint32_t i = 0; i < workers; i++) {
        uint32_t ret_val;

        err = sc_thread_join(full_join_vector(), NULL, (void **)&ret_val);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_UINT(0, ret_val);
    }

    TEST_SUCCEED();
}


bool test_syscall_fut(void) {
    BEGIN_SUITE("Syscall Futex");
    RUN_TEST(test_futex0);
    RUN_TEST(test_futex_early_destruct);
    return END_SUITE();
}
