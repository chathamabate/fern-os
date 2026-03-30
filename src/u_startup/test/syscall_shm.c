
#include "u_startup/test/syscall_shm.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall_pipe.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

/*
 * Taken from another test, this should really be defined somewhere shared
 */
static fernos_error_t reap_single(proc_id_t rcpid) {
    fernos_error_t err;
    proc_exit_status_t rces;

    while (1) {
        err = sc_proc_reap(rcpid, NULL, &rces);
        if (err == FOS_E_SUCCESS) {
            // We clear just in case we were able to reap without waiting on the signal!
            // If we didn't clear, the FSIG_CHLD bit may still be set after returning from this
            // function. Although, in reality, I don't think that would be such a problem.
            sc_signal_clear(1 << FSIG_CHLD);

            if (rces != PROC_ES_SUCCESS) {
                return FOS_E_UNKNWON_ERROR;
            }

            return FOS_E_SUCCESS;
        }

        if (err != FOS_E_EMPTY) {
            return err;
        }

        err = sc_signal_wait((1 << FSIG_CHLD), NULL);
        if (err != FOS_E_SUCCESS) {
            return err;
        }
    }
}

/*
 * `test_sem_buffer` will help with initial semaphore testing local to a single
 * process!
 */

#define TEST_SEM_BUFFER_LEN 8
static uint32_t test_sem_buffer[TEST_SEM_BUFFER_LEN];


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

        // Each iteration, we increment each cell in the buffer by 1.
        // AND confirm the buffer is uniform!
        //
        // The sleep is provoke the potential for data races. 
        // (Which shouldn't be a problem if semaphores work!)
        const uint32_t val = test_sem_buffer[0];
        for (size_t i = 0; i < TEST_SEM_BUFFER_LEN; i++) {
            sc_thread_sleep(1);
            TEST_EQUAL_UINT(val, test_sem_buffer[i]);
            test_sem_buffer[i] = val + 1;
        }

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

/*
 * For cross process tests we'll use pipes here.
 *
 * The idea being we can prove semaphores work before getting shared memory involved.
 * (This is assuming pipes work of course!)
 */

typedef struct {
    sem_id_t sem;

    handle_t wp, rp;
    size_t pipe_size;

    size_t iters;
} tscpw_arg_t;

static void *test_sem_cross_process_worker(void *arg) {
    const tscpw_arg_t * const targ = arg;

    for (size_t i = 0; i < targ->iters; i++) {
        TEST_SUCCESS(sc_shm_sem_dec(targ->sem));

        // This is just like the process individual test, but now, we'll read from a pipe!
        // We'll read one byte at a time, once again to provoke memory issues!
        // We won't add sleeps because writing to the pipe should provide sufficient slow down.

        size_t txed;

        uint8_t ref_byte;
        uint8_t tmp_byte;

        TEST_SUCCESS(sc_handle_read(targ->rp, &ref_byte, 1, &txed));
        TEST_EQUAL_UINT(1, txed);

        for (size_t i = 0; i < targ->pipe_size - 1; i++) {
            TEST_SUCCESS(sc_handle_read(targ->rp, &tmp_byte, 1, &txed));
            TEST_EQUAL_UINT(1, txed);
            TEST_EQUAL_UINT(ref_byte, tmp_byte);
        }

        // The pipe should not hold excess bytes!
        TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_read(targ->rp, &tmp_byte, 1, &txed));

        // Now we write back the incremented version.
        ref_byte++;

        for (size_t i = 0; i < targ->pipe_size; i++) {
            TEST_SUCCESS(sc_handle_write(targ->wp, &ref_byte, 1, &txed));
            TEST_EQUAL_UINT(1, txed);
        }

        sc_shm_sem_inc(targ->sem);
    }

    return NULL;
}

static bool test_sem_cross_process(void) {
    // Semaphores should behave properly when workers are in different processes!
    
    proc_id_t cpids[2];
    thread_id_t tids[3];

    const size_t pipe_size = 16;
    const size_t num_children = sizeof(cpids) / sizeof(cpids[0]);
    const size_t workers_per_child = sizeof(tids) / sizeof(tids[0]);
    const size_t iters_per_worker = 4;

    sig_vector_t sv = sc_signal_allow(1 << FSIG_CHLD);

    handle_t wp, rp;

    TEST_SUCCESS(sc_pipe_open(&wp, &rp, pipe_size));

    // First things first, let's fill up our pipe with 0s.
    // Each worker of each child process will individually empty and refill the pipe for a 
    // certain number of iterations.

    const uint8_t first_byte = 0;
    size_t written;
    for (size_t i = 0; i < pipe_size; i++) {
        TEST_SUCCESS(sc_handle_write(wp, &first_byte, 1, &written));
        TEST_EQUAL_HEX(1, written); // This check of 1 is kinda redundant, but whatever.
    }

    sem_id_t sem;

    TEST_SUCCESS(sc_shm_new_semaphore(&sem, 1));

    const tscpw_arg_t arg = {
        .sem = sem,
        .wp = wp, .rp = rp,
        .pipe_size = pipe_size,
        .iters = iters_per_worker
    };

    for (size_t i = 0; i < num_children; i++) {
        TEST_SUCCESS(sc_proc_fork(cpids + i));

        if (cpids[i] == FOS_MAX_PROCS) { // child process!
            // For each child process we spawn workers! 
            for (size_t j = 0; j < workers_per_child; j++) {
                TEST_SUCCESS(sc_thread_spawn(tids + j, test_sem_cross_process_worker, (void *)&arg));
            }

            for (size_t j = 0; j < workers_per_child; j++) {
                TEST_SUCCESS(sc_thread_join(1 << tids[j], NULL, NULL));
            }

            sc_proc_exit(PROC_ES_SUCCESS);
        }
    }

    // Parent process.

    // Now we reap every child!
    for (size_t i = 0; i < num_children; i++) {
        TEST_SUCCESS(reap_single(cpids[i])); 
    }

    // At the very end, confirm our data!

    TEST_SUCCESS(sc_shm_sem_dec(sem)); // The semaphore should be free!

    const uint8_t exp_byte = num_children * workers_per_child * iters_per_worker;
    uint8_t act_byte;
    size_t readden;

    for (size_t i = 0; i < pipe_size; i++) {
        TEST_SUCCESS(sc_handle_read(rp, &act_byte, 1, &readden));
        TEST_EQUAL_UINT(1, readden);
        TEST_EQUAL_UINT(exp_byte, act_byte);
    }

    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_read(rp, &act_byte, 1, NULL));

    sc_shm_sem_inc(sem);

    sc_shm_close_semaphore(sem);
    sc_handle_close(wp);
    sc_handle_close(rp);

    sc_signal_allow(sv);

    TEST_SUCCEED();
}

bool test_syscall_shm_sem(void) {
    BEGIN_SUITE("Shared Memory: Semaphores");
    RUN_TEST(test_sem_new_and_close);
    RUN_TEST(test_sem_simple_lock);
    RUN_TEST(test_sem_cross_process);
    return END_SUITE();
}
