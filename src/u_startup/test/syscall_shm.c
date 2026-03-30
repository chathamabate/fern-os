
#include "u_startup/test/syscall_shm.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

/*
 * `test_sem_buffer` will help with initial semaphore testing local to a single
 * process!
 */

#define TEST_SEM_BUFFER_LEN 8
static uint32_t test_sem_buffer[TEST_SEM_BUFFER_LEN];

static bool test_sem_buf_inc(void) {
    const uint32_t val = test_sem_buffer[0];

    for (size_t i = 0; i < TEST_SEM_BUFFER_LEN; i++) {
        sc_thread_sleep(1);
        TEST_EQUAL_UINT(val, test_sem_buffer[i]);
        test_sem_buffer[i] = val + 1;
    }

    TEST_SUCCEED();
}

static bool test_sem_new_and_close(void) {
    sem_id_t sem;

    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_new_semaphore(NULL, 10));
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_new_semaphore(&sem, 0));

    TEST_SUCCESS(sc_shm_new_semaphore(&sem, 2));
    sc_shm_close_semaphore(sem);

    TEST_SUCCEED();
}

static void *test_sem_simple_lock_worker(void *arg) {
    sem_id_t sem = (sem_id_t)arg;

    for (size_t i = 0; i < 5; i++) {
        TEST_SUCCESS(sc_shm_sem_dec(sem));
        ENTER_SUBTEST(test_sem_buf_inc);
        sc_shm_sem_inc(sem);
    }

    return NULL;
}

static bool test_sem_simple_lock(void) {
    sem_id_t sem;

    TEST_SUCCESS(sc_shm_new_semaphore(&sem, 1));

    mem_set(test_sem_buffer, 0, sizeof(test_sem_buffer));

    // This test will be akin to a simple mutex test, spawn a bunch of workers,
    // all which modify a shared buffer.

    thread_id_t tids[10];
    const size_t num_workers = sizeof(tids) / sizeof(tids[0]);

    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_spawn(tids + i, test_sem_simple_lock_worker, (void *)sem));
    }

    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_join(1U << tids[i], NULL, NULL));
    }

    for (size_t i = 0; i < TEST_SEM_BUFFER_LEN; i++) {
        TEST_EQUAL_UINT(num_workers * 5, test_sem_buffer[i]);
    }

    sc_shm_close_semaphore(sem);

    TEST_SUCCEED();
}

static bool test_sem_cross_process(void) {
    // Semaphores should behave properly when workers are in different processes!
    
    sem_id_t sem;

    TEST_SUCCESS(sc_shm_new_semaphore(&sem, 1));

    sc_shm_close_semaphore(sem);

    TEST_SUCCEED();
}

bool test_syscall_shm_sem(void) {
    BEGIN_SUITE("Shared Memory: Semaphores");
    RUN_TEST(test_sem_new_and_close);
    RUN_TEST(test_sem_simple_lock);
    return END_SUITE();
}
