

#include "u_startup/test/syscall_exec.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "s_util/constraints.h"

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

bool test_syscall_exec(void) {
    BEGIN_SUITE("Syscall Exec");
    RUN_TEST(test_exit_status);
    RUN_TEST(test_adoption);
    return END_SUITE();
}
