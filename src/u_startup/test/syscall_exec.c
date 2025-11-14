

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

static void *test_thread_wait_worker(void *arg) {
    handle_t h = *(handle_t *)arg;
    sc_handle_wait(h);
    return NULL;
}

static void *test_thread_futex_worker(void *arg) {
    futex_t *fut = (futex_t *)arg;
    sc_fut_wait(fut, 0); // Wait forever.
    return NULL;
}

static void *test_thread_exec_worker(void *arg) {
    switch ((uintptr_t)arg) {
    case 0: // Return right away.
        return NULL;
    case 1: // Run forever.
        while (1);
    default: // Exec the test application.
        TEST_SUCCESS(sc_fs_exec_da_elf32_va(TEST_APP));
        return NULL; // Should never make it here.
    }
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
            TEST_SUCCESS(sc_fut_register(futs + i));
        }

        // Two threads waiting on each futex.
        for (size_t i = 0; i < num_futs * 2; i++) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_thread_futex_worker, futs + (i % num_futs)));
        }

        handle_t h;
        TEST_SUCCESS(sc_fs_open(data_pipe_path, &h));

        // 3 threads just waiting on a handle. (Multiple threads ARE allowed to wait on the 
        // same file handle) On exec, the handle being waited on should be cleaned up!
        for (size_t i = 0; i < 3; i++) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_thread_wait_worker, &h));
        }

        // 2 threads to return right away.
        for (size_t i = 0; i < 2; i++) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_thread_exec_worker, (void *)0));
        }

        // 2 threads to run forever.
        for (size_t i = 0; i < 2; i++) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_thread_exec_worker, (void *)1));
        }

        // This will call exec from a NON MAIN THREAD!
        TEST_SUCCESS(sc_thread_spawn(NULL, test_thread_exec_worker, (void *)2));

        while (1);
    }

    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

    proc_exit_status_t es;
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, &es));
    TEST_EQUAL_UINT(100, es);

    TEST_SUCCESS(sc_fs_remove(data_pipe_path));

    TEST_SUCCEED();
}

static bool test_default_io(void) {
    TEST_SUCCESS(sc_fs_touch("h2c"));
    TEST_SUCCESS(sc_fs_touch("c2h"));

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) {
        handle_t d_in;
        TEST_SUCCESS(sc_fs_open("h2c", &d_in));
        sc_set_in_handle(d_in);

        handle_t d_out;
        TEST_SUCCESS(sc_fs_open("c2h", &d_out));
        sc_set_out_handle(d_out);

        // IO in a loop forever!
        TEST_SUCCESS(sc_fs_exec_da_elf32_va(TEST_APP, "b"));

        while (1); // should never make it here.
    }

    handle_t h2c;
    TEST_SUCCESS(sc_fs_open("h2c", &h2c));

    handle_t c2h;
    TEST_SUCCESS(sc_fs_open("c2h", &c2h));

    char buf[100];

    TEST_SUCCESS(sc_handle_write_full(h2c, "abc", 3));
    TEST_SUCCESS(sc_handle_read_full(c2h, buf, 3));
    TEST_TRUE(mem_cmp("bcd", buf, 3));

    TEST_SUCCESS(sc_handle_write_full(h2c, "aaabbb", 6));
    TEST_SUCCESS(sc_handle_read_full(c2h, buf, 6));
    TEST_TRUE(mem_cmp("bbbccc", buf, 6));

    sc_handle_close(c2h);
    sc_handle_close(h2c);

    TEST_SUCCESS(sc_signal(cpid, 1));
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

    proc_exit_status_t es;
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, &es));
    TEST_EQUAL_HEX(PROC_ES_SIGNAL, es);

    TEST_SUCCESS(sc_fs_remove("c2h"));
    TEST_SUCCESS(sc_fs_remove("h2c"));
    TEST_SUCCEED();
}

bool test_syscall_exec(void) {
    BEGIN_SUITE("Syscall Exec");
    RUN_TEST(test_exit_status);
    RUN_TEST(test_adoption);
    RUN_TEST(test_big_adoption);
    RUN_TEST(test_mutli_threaded);
    RUN_TEST(test_default_io);
    return END_SUITE();
}
