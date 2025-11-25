
#include "u_startup/syscall.h"
#include "u_startup/syscall_pipe.h"
#include "u_startup/test/syscall_pipe.h"
#include "s_util/constraints.h"
#include "s_util/misc.h"
#include "s_util/rand.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

/*
 * Realize that unlike with file handles, it is completely OK to read/write from the same 
 * handle interchangeably. (Even across multiple threads)
 */

typedef struct _pipe_test_arg_t {
    handle_t p;

    // Misc fields to use depending on the test.
    uint32_t arg0;
    uint32_t arg1;
} pipe_test_arg_t;

static bool test_simple_rw0(void) {
    handle_t wp, rp;

    TEST_SUCCESS(sc_pipe_open(&wp, &rp, 16));

    char buf[10];

    // Nothing to read, totally ok!
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_read(rp, buf, sizeof(buf), NULL));

    // Pipe should be ready to write!
    TEST_SUCCESS(sc_handle_wait_write_ready(wp));

    const char *msg = "Bob";
    TEST_SUCCESS(sc_handle_write_s(wp, msg)); // Remember, this doesn't write NT.

    TEST_SUCCESS(sc_handle_read_full(rp, buf, str_len(msg)));
    buf[str_len(msg)] = '\0';

    TEST_TRUE(str_eq(msg, buf));

    sc_handle_close(wp);
    sc_handle_close(rp);

    TEST_SUCCEED();
}

static bool test_simple_end_cases(void) {
    handle_t wp, rp;
    TEST_SUCCESS(sc_pipe_open(&wp, &rp, 16));

    sc_handle_close(rp);

    // When a read handle is closed, we shouldn't be able to give any data at all to the 
    // write end.
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_wait_write_ready(wp));
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_write(wp, "Hello", 5, NULL));

    sc_handle_close(wp);

    TEST_SUCCESS(sc_pipe_open(&wp, &rp, 16));

    TEST_SUCCESS(sc_handle_write_s(wp, "hello"));

    sc_handle_close(wp);

    char buf[16];

    // When there are no write handles, we should still be able to read whatever data is left!
    TEST_SUCCESS(sc_handle_read_full(rp, buf, 5));
    TEST_TRUE(mem_cmp(buf, "hello", 5));

    // Now that the pipe is empty, reading/read waiting should return empty!
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_read(rp, buf, 4, NULL));
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_wait_read_ready(rp));

    sc_handle_close(rp);

    TEST_SUCCEED();
}

static void *test_multithread_rw0_reader(void *arg) {
    TEST_TRUE(arg != NULL);

    pipe_test_arg_t *pipe_arg = (pipe_test_arg_t *)arg;
    const handle_t p = pipe_arg->p;
    const size_t trials = pipe_arg->arg0;

    uint8_t r_buf[255];
    for (size_t trial = 0; trial < trials; trial++) {
        uint8_t len;
        TEST_SUCCESS(sc_handle_read_full(p, &len, sizeof(len)));

        if (len > 0) {
            TEST_SUCCESS(sc_handle_read_full(p, r_buf, len));

            const uint8_t first_val = r_buf[0];
            for (size_t i = 0; i < len; i++) {
                TEST_EQUAL_UINT((uint8_t)(first_val + i), r_buf[i]);
            }
        }
    }

    return NULL;
}

static bool test_multithread_rw0(void) {
    handle_t wp;
    handle_t rp;

    const uint32_t num_trials = 100;

    rand_t r = rand(0);

    TEST_SUCCESS(sc_pipe_open(&wp, &rp,  100));

    pipe_test_arg_t pipe_arg = {
        rp, num_trials, 0
    };

    thread_id_t tid;
    TEST_SUCCESS(sc_thread_spawn(&tid, test_multithread_rw0_reader, &pipe_arg));

    // 1 for length, 255 for values.
    uint8_t w_buf[256];

    // Random test time!
    for (size_t trial = 0; trial < num_trials; trial++) { // run the same thing a bunch of times.
        const uint8_t len = next_rand_u8(&r);
        const uint8_t val = next_rand_u8(&r);
        
        w_buf[0] = len;
        for (size_t i = 1; i <= len; i++) {
            w_buf[i] = (uint8_t)(val + i);
        }
        TEST_SUCCESS(sc_handle_write_full(wp, w_buf, 1 + len));
    }

    TEST_SUCCESS(sc_thread_join(1 << tid, NULL, NULL));

    sc_handle_close(wp);
    sc_handle_close(rp);

    TEST_SUCCEED();
}


static void *test_multithread_rw1_writer(void *arg) {
    pipe_test_arg_t *pipe_arg = (pipe_test_arg_t *)arg;

    handle_t p = pipe_arg->p;
    uint32_t len = pipe_arg->arg0;

    uint8_t buf[200];
    TEST_TRUE(len < sizeof(buf));

    mem_set(buf, 0xAB, len);

    TEST_SUCCESS(sc_handle_write_full(p, buf, len));

    return NULL;
}

static void *test_multithread_rw1_reader(void *arg) {
    pipe_test_arg_t *pipe_arg = (pipe_test_arg_t *)arg;

    handle_t p = pipe_arg->p;
    uint32_t len = pipe_arg->arg0;

    uint8_t buf[200];
    TEST_TRUE(len < sizeof(buf));

    mem_set(buf, 0, len);

    TEST_SUCCESS(sc_handle_read_full(p, buf, len));
    TEST_TRUE(mem_chk(buf, 0xAB, len));

    return NULL;
}

static bool test_multithread_rw1(void) {
    // I'd like to test read waiting AND write waiting!

    handle_t wp;
    handle_t rp;
    TEST_SUCCESS(sc_pipe_open(&wp, &rp, 16));

    pipe_test_arg_t write_args[4] = {
        {wp, 10, 0},
        {wp, 20, 0},
        {wp, 45, 0},
        {wp, 15, 0}
        // A total of 90 bytes.
    };
    const size_t num_write_args = sizeof(write_args) / sizeof(write_args[0]);

    pipe_test_arg_t read_args[5] = {
        {rp, 10, 0},
        {rp, 50, 0},
        {rp, 5, 0},
        {rp, 5, 0},
        {rp, 20, 0}
        // Also a total of 90 bytes!
    };
    const size_t num_read_args = sizeof(read_args) / sizeof(read_args[0]);

    // We'll try spawning in differnet orders to see what happens!

    // 1) Writers then readers.
    for (size_t i = 0; i < num_write_args; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_writer, write_args + i));
    }
    sc_thread_sleep(5); // Small sleep cause why not.
    for (size_t i = 0; i < num_read_args; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_reader, read_args + i));
    }
    for (size_t i = 0; i < num_write_args + num_read_args; i++) {
        TEST_SUCCESS(sc_thread_join(full_join_vector(), NULL, NULL));
    }

    // 2) Readers then writers.
    for (size_t i = 0; i < num_read_args; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_reader, read_args + i));
    }
    sc_thread_sleep(5); // Small sleep cause why not.
    for (size_t i = 0; i < num_write_args; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_writer, write_args + i));
    }
    for (size_t i = 0; i < num_write_args + num_read_args; i++) {
        TEST_SUCCESS(sc_thread_join(full_join_vector(), NULL, NULL));
    }

    // 3) intermixed.
    for (size_t i = 0; i < MAX(num_read_args, num_write_args); i++) {
        if (i < num_read_args) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_reader, read_args + i));
        }
        if (i < num_write_args) {
            TEST_SUCCESS(sc_thread_spawn(NULL, test_multithread_rw1_writer, write_args + i));
        }
        sc_thread_sleep(1);
    }
    for (size_t i = 0; i < num_write_args + num_read_args; i++) {
        TEST_SUCCESS(sc_thread_join(full_join_vector(), NULL, NULL));
    }

    sc_handle_close(wp);
    sc_handle_close(rp);

    TEST_SUCCEED();
}

static bool test_multiproc(void) {
    // This primarily tests copy constructor and destructor for pipes. 

    sig_vector_t old_sv = sc_signal_allow(full_sig_vector());

    handle_t wp, rp;
    sc_pipe_open(&wp, &rp, 20);

    char buf[6]; 

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) { // child proc.
        TEST_SUCCESS(sc_handle_read_full(rp, buf, 6));
        TEST_TRUE(str_eq("Hello", buf));

        sc_signal(FOS_MAX_PROCS, 5);

        TEST_SUCCESS(sc_handle_write_full(wp, "Heheh", 6));

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    TEST_SUCCESS(sc_handle_write_full(wp, "Hello", 6));
    TEST_SUCCESS(sc_signal_wait(1 << 5, NULL));
    TEST_SUCCESS(sc_handle_read_full(rp, buf, 6));
    TEST_TRUE(str_eq("Heheh", buf));

    sc_handle_close(wp);
    sc_handle_close(rp);

    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

    proc_exit_status_t res;
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, &res));
    TEST_EQUAL_HEX(FOS_E_SUCCESS, res);

    sc_signal_allow(old_sv);

    TEST_SUCCEED();
}

static const char *MULTIPROC_COMPLEX0_CASES[] = {
    "hello",
    "AYE Yo 123",
    "AAAAAAAAAAAAAAAAAAaaaaaaaaaaaaaaaaaaaBBBBBBBBBBBBBBCVCCCCCCCCCCCCCCcc",
    "WOOOOOOOO 123213123 hello woooo what is up ???????"
};
static const size_t MULTIPROC_COMPLEX0_CASES_LEN = sizeof(MULTIPROC_COMPLEX0_CASES) / sizeof(MULTIPROC_COMPLEX0_CASES[0]);

static void *test_multiproc_complex0_worker(void *arg) {
    TEST_TRUE(arg != NULL);
    handle_t wp = *(handle_t *)arg; 

    for (size_t i = 0; i < MULTIPROC_COMPLEX0_CASES_LEN; i++) {
        // This will probably block, as it will take time for the reader to process input!
        // This is expected and desired!
        TEST_SUCCESS(sc_handle_write_s(wp, MULTIPROC_COMPLEX0_CASES[i]));
    }

    // The worker will close the handle when it's done!
    sc_handle_close(wp);
    
    return NULL;
}

static bool test_multiproc_complex0(void) {
    // this will be like a test I wrote for the file system.
    
    // parent -> pipe0 ----*
    //                     |
    //               Child (does some sort of data manipulation)
    //                     |
    // parent <- pipe1 ----*

    fernos_error_t err;

    sig_vector_t old_sv = sc_signal_allow(1 << FSIG_CHLD);

    handle_t wp0, rp0;
    TEST_SUCCESS(sc_pipe_open(&wp0, &rp0, 16));

    handle_t rp1, wp1;
    TEST_SUCCESS(sc_pipe_open(&wp1, &rp1, 15));

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));


    if (cpid == FOS_MAX_PROCS) {
        sc_handle_close(wp0);
        sc_handle_close(rp1);

        char child_buf[16];

        while (true) {
            size_t readden;
            while ((err = sc_handle_read(rp0, child_buf, sizeof(child_buf), &readden)) == FOS_E_SUCCESS) {
                for (size_t i = 0; i < readden; i++) {
                    if ('a' <= child_buf[i] && child_buf[i] <= 'z') {
                        child_buf[i] += 'A' - 'a'; // Capitalize!
                    }
                }

                TEST_SUCCESS(sc_handle_write_full(wp1, child_buf, readden));
            }

            if (err == FOS_E_EMPTY) {
                err = sc_handle_wait_read_ready(rp0);

                // We expect that at a certain point the parent process will close its write end on
                // pipe0. This should cause FOS_E_EMPTY to be returned from wait read ready!
                if (err != FOS_E_SUCCESS) {
                    sc_proc_exit(err == FOS_E_EMPTY ? PROC_ES_SUCCESS : PROC_ES_FAILURE);
                }
            } else {
                // Anything other then success or Empty from the read call is an automatic failure!
                sc_proc_exit(PROC_ES_FAILURE);
            }
        }
    }

    sc_handle_close(rp0);
    sc_handle_close(wp1);

    thread_id_t worker_tid;
    TEST_SUCCESS(sc_thread_spawn(&worker_tid, test_multiproc_complex0_worker, &wp0));
    (void)wp0; // From this point on the worker owns `wp0`! It will close it when done!

    char parent_buf[1024];
    for (size_t i = 0; i < MULTIPROC_COMPLEX0_CASES_LEN; i++) {
        const char *c = MULTIPROC_COMPLEX0_CASES[i];
        const size_t slen = str_len(c);

        TEST_SUCCESS(sc_handle_read_full(rp1, parent_buf, str_len(c)));

        for (size_t j = 0; j < slen; j++) {
            char exp = c[j];
            if ('a' <= exp && exp <= 'z') {
                exp += 'A' - 'a';
            }
            TEST_EQUAL_UINT(exp, parent_buf[j]);
        }
    }

    // When we've read everything out, we can join on our worker!
    TEST_SUCCESS(sc_thread_join(1 << worker_tid, NULL, NULL));

    // Next we can wait and reap our child process!
    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));

    proc_exit_status_t rces;
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, &rces));
    TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);

    // Finally, we can close the last remaining open pipe handle `rp1`
    sc_handle_close(rp1);

    sc_signal_allow(old_sv);

    TEST_SUCCEED();
}

bool test_syscall_pipe(void) {
    BEGIN_SUITE("Syscall Pipe");
    RUN_TEST(test_simple_rw0);
    RUN_TEST(test_simple_end_cases);
    RUN_TEST(test_multithread_rw0);
    RUN_TEST(test_multithread_rw1);
    RUN_TEST(test_multiproc);
    RUN_TEST(test_multiproc_complex0);
    return END_SUITE();
}
