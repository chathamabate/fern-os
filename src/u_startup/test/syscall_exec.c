

#include "u_startup/test/syscall_exec.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/syscall_fut.h"
#include "s_util/constraints.h"

/*
 * NOTE: These tests kinda cover the behavior of a few different files/plugins.
 * (i.e. the file system plugin and the futex plugin)
 */

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

#define TEST_APP "/test_apps/Test"

#define _SYSCALL_EXEC_TEST_DIR "syscall_exec_test_dir"
#define SYSCALL_EXEC_TEST_DIR "/" _SYSCALL_EXEC_TEST_DIR

sig_vector_t old_sv;

static bool pretest(void) {
    TEST_SUCCESS(sc_fs_mkdir(SYSCALL_EXEC_TEST_DIR)); 
    TEST_SUCCESS(sc_fs_set_wd(SYSCALL_EXEC_TEST_DIR));
    
    old_sv = sc_signal_allow(1 << FSIG_CHLD);

    TEST_SUCCEED();
}

static bool posttest(void) {
    sc_signal_allow(old_sv);

    TEST_SUCCESS(sc_fs_set_wd("/"));
    TEST_SUCCESS(sc_fs_remove(SYSCALL_EXEC_TEST_DIR));

    TEST_SUCCEED();
}

static bool test_exit_status(void) {
    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) {
        TEST_SUCCESS(sc_fs_exec_da_elf32_va(TEST_APP));

        // Will never make it here.
    }
    
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

    proc_exit_status_t es;
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, &es));

    TEST_EQUAL_UINT(100, es); // Expected return status with 0 args.

    TEST_SUCCEED();
}

static bool test_adoption(void) {
    // After running exec, all child processes should be adopted by the root process!

    // We will use a file to communicate with grandchildren!
    const char *data_pipe_path = "data_pipe";
    TEST_SUCCESS(sc_fs_touch(data_pipe_path));

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) {
        sc_signal_allow(1 << FSIG_CHLD);

        proc_id_t gcpid;

        // First grand child will immediately exit.
        TEST_SUCCESS(sc_proc_fork(&gcpid));
        if (gcpid == FOS_MAX_PROCS) {
            sc_proc_exit(PROC_ES_SUCCESS);
        }

        // Here we wait to confirm it is a zombie.
        TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

        // Second grand child waits for a single character across the data pipe!
        // Then exits!
        TEST_SUCCESS(sc_proc_fork(&gcpid));
        if (gcpid == FOS_MAX_PROCS) {
            handle_t gc_dp;
            TEST_SUCCESS(sc_fs_open(data_pipe_path, &gc_dp));

            char c;
            TEST_SUCCESS(sc_handle_read_full(gc_dp, &c, sizeof(c)));

            sc_handle_close(gc_dp);

            sc_proc_exit(PROC_ES_SUCCESS);
        }

        // With arg "a", the test application never exits.
        // When this call executes, exactly one zombie process and one child process should
        // be adopted by the root process!
        TEST_SUCCESS(sc_fs_exec_da_elf32_va(TEST_APP, "a"));
    }
    
    // Ok, back in the root. 
    
    // First let's reap the only zombie.
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));
    TEST_SUCCESS(sc_proc_reap(FOS_MAX_PROCS, NULL, NULL));

    // Next, let's send a character over the data pipe.

    handle_t dp;
    TEST_SUCCESS(sc_fs_open(data_pipe_path, &dp));
    TEST_SUCCESS(sc_handle_write_full(dp, "a", 1));
    sc_handle_close(dp);

    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));
    TEST_SUCCESS(sc_proc_reap(FOS_MAX_PROCS, NULL, NULL));

    // Finally, Let's signal our original child!
    TEST_SUCCESS(sc_signal(cpid, 1));
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL)); // Wait for forced exit.
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, NULL));

    // Finally, delete the data pipe, and call it a day.
    TEST_SUCCESS(sc_fs_remove(data_pipe_path));

    TEST_SUCCEED();
}

static bool test_big_adoption(void) {
    // Maybe test adopting like many living processes, should work fine given above test works.
    // Honestly, this test is almost a copy of the above test, keeping both here though because
    // eh.

    const char *data_pipe_path = "data_pipe";
    TEST_SUCCESS(sc_fs_touch(data_pipe_path));

    // This time, we'll open the data pipe handle here, because why not?
    handle_t dp;
    TEST_SUCCESS(sc_fs_open(data_pipe_path, &dp));

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    const size_t NUM_GRANDCHILDREN = 5;
    if (cpid == FOS_MAX_PROCS) {
        sc_signal_allow(1 << FSIG_CHLD);
        proc_id_t gcpid;

        // We will have one zombie, kinda like in the above test.
        // This will signal to the root when adoption has occurred.

        TEST_SUCCESS(sc_proc_fork(&gcpid));
        if (gcpid == FOS_MAX_PROCS) {
            sc_handle_close(dp);
            sc_proc_exit(PROC_ES_SUCCESS);
        }
        TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

        for (size_t i = 0; i < NUM_GRANDCHILDREN; i++) {
            TEST_SUCCESS(sc_proc_fork(&gcpid));
            if (gcpid == FOS_MAX_PROCS) {
                char c;
                TEST_SUCCESS(sc_handle_read_full(dp, &c, sizeof(c)));
                sc_handle_close(dp);

                sc_proc_exit(PROC_ES_SUCCESS);
            }
        }

        // Run forever until signaled!
        TEST_SUCCESS(sc_fs_exec_da_elf32_va(TEST_APP, "a"));
    }

    // Ok, now for reap/waiting.

    // Reap first zombie.
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));
    TEST_SUCCESS(sc_proc_reap(FOS_MAX_PROCS, NULL, NULL));

    // Write to data pipe.
    sc_handle_write_full(dp, "a", 1);

    // Now reap all grand children.
    
    size_t reaped = 0;

    while (reaped < NUM_GRANDCHILDREN) {
        TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

        fernos_error_t err;
        while ((err = sc_proc_reap(FOS_MAX_PROCS, NULL, NULL)) == FOS_E_SUCCESS) {
            reaped++;
        }

        TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    }
    sc_signal_clear(1 << FSIG_CHLD);

    // Finally, kill direct child.
    TEST_SUCCESS(sc_signal(cpid, 1));
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, NULL));

    sc_handle_close(dp);
    TEST_SUCCESS(sc_fs_remove(data_pipe_path));

    TEST_SUCCEED();
}

static void *test_thread_read_worker(void *arg) {
    handle_t h = *(handle_t *)arg;

    char c;
    sc_handle_read_full(h, &c, sizeof(c)); // Read forever.

    return NULL;
}

static void *test_thread_futex_worker(void *arg) {
    futex_t *fut = (futex_t *)arg;
    sc_fut_wait(fut, 0); // Wait forever.
    return NULL;
}

static void *test_thread_do_nothing_worker(void *arg) {
    if (arg) {
        return arg; // Just exit!
    }

    while (1); // DO NOTHING!
}

static bool test_mutli_threaded(void) {
    // On exec, all threads of the calling process should be deleted or overwritten.

    // We will spawn a few threads, some will run forever, some will wait on a file handle,
    // some will wait on futexes. On exec, all should be cleaned up correctly.

    const char *data_pipe_path = "data_pipe";
    TEST_SUCCESS(sc_fs_touch(data_pipe_path));

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) {
        futex_t futs[3] = {0};
        const size_t num_futs = sizeof(futs) / sizeof(futs[0]);

        for (size_t i = 0; i < num_futs; i++) {

        }
          
        // What happens when two threads wait on the same handle??
    }


    TEST_SUCCESS(sc_fs_remove(data_pipe_path));

    TEST_SUCCEED();
}

// Default IO? Multi threading? Then what??

bool test_syscall_exec(void) {
    BEGIN_SUITE("Syscall Exec");
    RUN_TEST(test_exit_status);
    RUN_TEST(test_adoption);
    RUN_TEST(test_big_adoption);
    return END_SUITE();
}
