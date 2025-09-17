
#include "u_concur/mutex.h"
#include "u_concur/test/mutex.h"

#include "u_startup/syscall.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_term_put_fmt_s(__VA_ARGS__)
//#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

/*
 * Since a mutex is really just a futex under the hood, we don't need to
 * do too much rigorous testing here.
 */

static mutex_t mut;
static uint32_t number;

#define TEST_MANY_WORKERS_WORKER_ITERS (5)
static void *test_many_workers_worker(void *arg) {
    (void)arg;

    fernos_error_t err;

    for (uint32_t i = 0; i < TEST_MANY_WORKERS_WORKER_ITERS; i++) {
        err = mut_lock(&mut); 
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        bool odd = (number & 1) == 1;

        sc_thread_sleep(1);

        if (odd) { // odd.
            number++;
        } else { // even.
            number += 3;
        }

        err = mut_unlock(&mut);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    return NULL;
}

static bool test_many_workers(void) {
    number = 0;

    fernos_error_t err;

    err = init_mutex(&mut);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    const uint32_t workers = 10;
    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_spawn(NULL, test_many_workers_worker, NULL);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    for (uint32_t i = 0; i < workers; i++) {
        err = sc_thread_join(full_join_vector(), NULL, NULL);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    cleanup_mutex(&mut);

    uint32_t total_iters = workers * TEST_MANY_WORKERS_WORKER_ITERS;

    TEST_EQUAL_UINT(total_iters * 2, number);

    TEST_SUCCEED();
}

static bool test_bad_unlock(void) {

    fernos_error_t err;

    err = init_mutex(&mut);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = init_mutex(&mut);
    TEST_TRUE(err != FOS_E_SUCCESS);

    err = mut_unlock(&mut);
    TEST_TRUE(err != FOS_E_SUCCESS);

    cleanup_mutex(&mut);

    TEST_SUCCEED();
}

bool test_mutex(void) {
    BEGIN_SUITE("Mutex");

    RUN_TEST(test_many_workers);
    RUN_TEST(test_bad_unlock);

    return END_SUITE();
}
