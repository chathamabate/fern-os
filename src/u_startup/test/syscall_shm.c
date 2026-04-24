
#include "u_startup/test/syscall_shm.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall_pipe.h"
#include "s_util/rand.h"
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
    TEST_SUCCESS(sc_shm_sem_dec(sem)); // Shouldn't block!
    sc_shm_close_semaphore(sem);

    // Shouldn't be useable after close!
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_sem_dec(sem));

    TEST_SUCCEED();
}

static void *test_sem_simple_lock_worker(void *arg) {
    fernos_error_t err;

    sem_id_t sem = (sem_id_t)arg;

    for (size_t i = 0; i < 5; i++) {
        err = sc_shm_sem_dec(sem);
        if (err != FOS_E_SUCCESS) {
            return (void *)1;
        }

        // Each iteration, we increment each cell in the buffer by 1.
        // AND confirm the buffer is uniform!
        //
        // The sleep is provoke the potential for data races. 
        // (Which shouldn't be a problem if semaphores work!)
        const uint32_t val = test_sem_buffer[0];
        for (size_t i = 0; i < TEST_SEM_BUFFER_LEN; i++) {
            sc_thread_sleep(1);
            if (val != test_sem_buffer[i]) {
                return (void *)1;
            }
            test_sem_buffer[i] = val + 1;
        }

        sc_shm_sem_inc(sem);
    }

    return (void *)0;
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

    void *worker_rv;
    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_join(1U << tids[i], NULL, &worker_rv));
        TEST_FALSE(worker_rv);
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
    fernos_error_t err;

    const tscpw_arg_t * const targ = arg;

    for (size_t i = 0; i < targ->iters; i++) {
        TEST_SUCCESS(sc_shm_sem_dec(targ->sem));

        // This is just like the process individual test, but now, we'll read from a pipe!
        // We'll read one byte at a time, once again to provoke memory issues!
        // We won't add sleeps because writing to the pipe should provide sufficient slow down.

        size_t txed;

        uint8_t ref_byte;
        uint8_t tmp_byte;

        err = sc_handle_read(targ->rp, &ref_byte, 1, &txed);
        if (err != FOS_E_SUCCESS || txed != 1) {
            return (void *)1;
        }

        for (size_t i = 0; i < targ->pipe_size - 1; i++) {
            err = sc_handle_read(targ->rp, &tmp_byte, 1, &txed);
            if (err != FOS_E_SUCCESS || 1 != txed || ref_byte != tmp_byte) {
                return (void *)1;
            }
        }

        // The pipe should not hold excess bytes!
        if (FOS_E_EMPTY != sc_handle_read(targ->rp, &tmp_byte, 1, &txed)) {
            return (void *)1;
        }

        // Now we write back the incremented version.
        ref_byte++;

        for (size_t i = 0; i < targ->pipe_size; i++) {
            err = sc_handle_write(targ->wp, &ref_byte, 1, &txed);
            if (err != FOS_E_SUCCESS || 1 != txed) {
                return (void *)1;
            }
        }

        sc_shm_sem_inc(targ->sem);
    }

    return (void *)0;
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

    // This is a little test line which confirms that a semaphore cannot be incremented past its
    // initialized count. If the semaphore actually does make it to a count of 2, the below
    // tests will likely yield incorret data!
    sc_shm_sem_inc(sem);

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

            void *worker_rv;
            for (size_t j = 0; j < workers_per_child; j++) {
                TEST_SUCCESS(sc_thread_join(1 << tids[j], NULL, &worker_rv));
                TEST_FALSE(worker_rv);
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

typedef struct {
    sem_id_t sem;
    bool exp_success;
} tsecw_arg_t;

static void *test_sem_early_close_worker(void *arg) {
    const tsecw_arg_t * const targ = (const tsecw_arg_t *)arg;

    fernos_error_t err = sc_shm_sem_dec(targ->sem);

    if (targ->exp_success) {
        if (err != FOS_E_SUCCESS) {
            return (void *)1;
        }
        sc_thread_sleep(1);
        sc_shm_sem_inc(targ->sem);
    } else if (err != FOS_E_STATE_MISMATCH) {
        return (void *)1;
    }

    return (void *)0;
}

static bool test_sem_early_close(void) {
    // This is testing the behavior where when a process closes a semaphore, all the process's
    // threads which are still waiting on the semaphore are rescheduled.
    //
    // Other waiting threads should not be affected!

    fernos_error_t err;

    sig_vector_t sv = sc_signal_allow(1 << FSIG_CHLD);

    sem_id_t sems[2];

    TEST_SUCCESS(sc_shm_new_semaphore(sems + 0, 1));
    TEST_SUCCESS(sc_shm_new_semaphore(sems + 1, 1));

    // Ok, before we do anything, let's decrement both semaphores to cause blocking in the 
    // below threads!

    TEST_SUCCESS(sc_shm_sem_dec(sems[0]));
    TEST_SUCCESS(sc_shm_sem_dec(sems[1]));

    thread_id_t tids[2]; // two threads per process!
    tsecw_arg_t wargs[2] = { // expect success in both workers by default!
        {sems[0], true},
        {sems[1], true}
    };

    proc_id_t cpid; 
    TEST_SUCCESS(sc_proc_fork(&cpid));

    void *worker_rv;

    if (cpid == FOS_MAX_PROCS) { // Child process!
        TEST_SUCCESS(sc_thread_spawn(tids + 0, test_sem_early_close_worker, wargs + 0));

        wargs[1].exp_success = false; // This guy will be woken up early!
        TEST_SUCCESS(sc_thread_spawn(tids + 1, test_sem_early_close_worker, wargs + 1));

        // Ok, we want to wait until worker 1 of the child proc is is blocked.
        // Unfortunately, we actually have no way of confirming this.
        // So, we'll just wait a little bit.
        //
        // Realize, this would fail if we end up closing the semaphore before worker 1 runs,
        // in this case state mismatch would not be returned, a bad args error would be!
        sc_thread_sleep(16);

        sc_shm_close_semaphore(sems[1]);
        TEST_SUCCESS(sc_thread_join(1 << tids[1], NULL, &worker_rv));
        TEST_FALSE(worker_rv);

        // Finally, we wait for the parent process to increment sem 0 enough times to
        // gaurantee a wake up of worker 0!
        
        TEST_SUCCESS(sc_thread_join(1 << tids[0], NULL, &worker_rv));
        TEST_FALSE(worker_rv);

        // This will be done autmatically, but whatever!
        sc_shm_close_semaphore(sems[0]);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // In the parent process, we'll just expect both workers on both sems to succeed.
    TEST_SUCCESS(sc_thread_spawn(tids + 0, test_sem_early_close_worker, wargs + 0));
    TEST_SUCCESS(sc_thread_spawn(tids + 1, test_sem_early_close_worker, wargs + 1));

    // Ok, let's get the child process to exit. It's logic on semaphore 1 is self-contained,
    // however we will need to manually increment semaphore 0 here before it can exit.
    // We must be careful though, as our thread 0 is also potentially waiting on semaphore 0.
    // 
    // We'll increment the semaphore until the child exits!

    while (true) {
        sc_shm_sem_inc(sems[0]);

        proc_exit_status_t rces;
        err = sc_proc_reap(cpid, NULL, &rces);
        if (err == FOS_E_SUCCESS) {
            TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);
            break;
        }

        // We expect that if our reap fails, that the child process is just still executing.
        // No biggy, just sleep and try again.
        //
        // We aren't using signal wait here as we want to be able to continue to increment 
        // the semaphore to gaurantee the child thread isn't blocked forever!
        TEST_EQUAL_HEX(FOS_E_EMPTY, err);

        sc_thread_sleep(1);
    }

    sc_signal_clear(1 << FSIG_CHLD);

    // Our child process has exited and been reaped!
    // We know it is impossible anyone else is contending for semaphore 0 or 1, increment them just
    // one more time to be certain our threads must wake up!
    sc_shm_sem_inc(sems[0]);
    sc_shm_sem_inc(sems[1]); // This is actually required for semaphore 1 as it gets 
                                  // incremented no where else!

    TEST_SUCCESS(sc_thread_join(1 << tids[0], NULL, &worker_rv));
    TEST_FALSE(worker_rv);

    TEST_SUCCESS(sc_thread_join(1 << tids[1], NULL, &worker_rv));
    TEST_FALSE(worker_rv);

    sc_shm_close_semaphore(sems[1]);
    sc_shm_close_semaphore(sems[0]);

    sc_signal_allow(sv);

    TEST_SUCCEED();
}

static bool test_sem_many(void) {
    // Here we just make sure we can make a bunch of semaphores and access them without error.
    sem_id_t sems[16]; 
    const size_t num_sems = sizeof(sems) / sizeof(sems[0]);

    for (size_t i = 0; i < num_sems; i++) {
        TEST_SUCCESS(sc_shm_new_semaphore(sems + i, 1));

        TEST_SUCCESS(sc_shm_sem_dec(sems[i]));
        sc_shm_sem_inc(sems[i]);
    }

    for (size_t i = 0; i < num_sems; i += 3) {
        sc_shm_close_semaphore(sems[i]);
    }

    for (size_t i = 0; i < num_sems; i++) {
        if (i % 3 == 0) {
            TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_sem_dec(sems[i]));
        } else {
            TEST_SUCCESS(sc_shm_sem_dec(sems[i]));
            sc_shm_sem_inc(sems[i]);

            sc_shm_close_semaphore(sems[i]);
        }
    }

    TEST_SUCCEED();
}

bool test_syscall_shm_sem(void) {
    BEGIN_SUITE("Shared Memory: Semaphores");
    RUN_TEST(test_sem_new_and_close);
    RUN_TEST(test_sem_simple_lock);
    RUN_TEST(test_sem_cross_process);
    RUN_TEST(test_sem_early_close);
    RUN_TEST(test_sem_many);
    return END_SUITE();
}

static bool test_new_shm_and_unmap(void) {
    const size_t shm_size = 5000;
    uint8_t *shm;

    TEST_SUCCESS(sc_shm_new_shm(shm_size, (void **)&shm));

    for (size_t i = 0; i < shm_size; i++) {
        shm[i] = (uint8_t)i;
    }

    for (size_t i = 0; i < shm_size; i++) {
        TEST_EQUAL_UINT((uint8_t)i, shm[i]);
    }

    sc_shm_close_shm(shm);

    TEST_SUCCEED();
}

static bool test_random_new_shm(void) {
    // Here we just randomly allocate shared memory areas and make sure nothing
    // breaks!
    
    struct {
        uint8_t occupied;

        uint8_t *start;
        uint32_t size;
    } shm_areas[30] = {0}; // All start as unoccupied.

    const size_t num_shm_areas = sizeof(shm_areas) / sizeof(shm_areas[0]);
    
    rand_t r = rand(0);

    for (size_t trial = 0; trial < 200; trial++) {
        uint32_t i = next_rand_u32(&r) % num_shm_areas;

        if (shm_areas[i].occupied) {
            sc_shm_close_shm(shm_areas[i].start);
        }
        
        uint32_t size = next_rand_u32(&r) & 0x1FFFF;
        TEST_SUCCESS(sc_shm_new_shm(size, (void **)&(shm_areas[i].start)));

        mem_set(shm_areas[i].start, (uint8_t)i, size);
        shm_areas[i].size = size;
        shm_areas[i].occupied = 1;
    }

    for (size_t i = 0; i < num_shm_areas; i++) {
        if (shm_areas[i].occupied) {
            TEST_TRUE(mem_chk(shm_areas[i].start, i, shm_areas[i].size)); 
            sc_shm_close_shm(shm_areas[i].start);
        }
    }

    TEST_SUCCEED();
}

static bool test_shm_exhaust(void) {
    // Now let's try allocating as many shm's as possible!

    fernos_error_t err;

    void *shms[100];
    const size_t max_shms = sizeof(shms) / sizeof(shms[0]);

    size_t i;

    for (i = 0; i < max_shms; i++) {
        err = sc_shm_new_shm(0x1000000, shms + i);
        if (err != FOS_E_SUCCESS) {
            break;
        }
    }
    
    TEST_TRUE(err != FOS_E_SUCCESS);

    for (size_t j = 0; j < i; j++) {
        sc_shm_close_shm(shms[j]);
    }

    TEST_SUCCEED();
}

// Ok, does unmap actually unmap??
// Can two processes really use the same shared memory at once??
// Can a parent unmap while a child still uses it??
// There are lots of questions!
// What if we really use shared memory to its limits?

bool test_syscall_shm(void) {
    BEGIN_SUITE("Shared Memory");
    RUN_TEST(test_new_shm_and_unmap);
    RUN_TEST(test_random_new_shm);
    RUN_TEST(test_shm_exhaust);
    return END_SUITE();
}
