
#include "u_startup/syscall.h"
#include "u_startup/syscall_pipe.h"
#include "u_startup/test/syscall_pipe.h"
#include "s_util/misc.h"
#include "s_util/rand.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

/*
 * Realize that unlike with file handles, it is completely OK to read/write from the same 
 * handle interchangeably. (Even across multiple threads)
 */

static size_t old_user_al;
static bool pretest(void) {
    TEST_TRUE(get_default_allocator() != NULL);
    old_user_al = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}

static bool posttest(void) {
    size_t new_user_al = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(old_user_al, new_user_al);
    TEST_SUCCEED();
}

typedef struct _pipe_test_arg_t {
    handle_t p;

    // Misc fields to use depending on the test.
    uint32_t arg0;
    uint32_t arg1;
} pipe_test_arg_t;

static bool test_simple_rw0(void) {
    handle_t p;

    TEST_SUCCESS(sc_pipe_open(&p, 16));

    char buf[10];

    // Nothing to read, totally ok!
    TEST_EQUAL_HEX(FOS_E_EMPTY, sc_handle_read(p, buf, sizeof(buf), NULL));

    // Pipe should be ready to write!
    TEST_SUCCESS(sc_handle_wait_write_ready(p));

    const char *msg = "Bob";
    TEST_SUCCESS(sc_handle_write_s(p, msg)); // Remember, this doesn't write NT.

    TEST_SUCCESS(sc_handle_read_full(p, buf, str_len(msg)));
    buf[str_len(msg) + 1] = '\0';

    TEST_TRUE(str_eq(msg, buf));

    sc_handle_close(p);

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
    handle_t p;
    const uint32_t num_trials = 100;

    rand_t r = rand(0);

    TEST_SUCCESS(sc_pipe_open(&p, 100));

    pipe_test_arg_t pipe_arg = {
        p, num_trials, 0
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
        TEST_SUCCESS(sc_handle_write_full(p, w_buf, 1 + len));
    }

    TEST_SUCCESS(sc_thread_join(1 << tid, NULL, NULL));

    sc_handle_close(p);

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

    handle_t p;
    TEST_SUCCESS(sc_pipe_open(&p, 16));

    pipe_test_arg_t write_args[4] = {
        {p, 10, 0},
        {p, 20, 0},
        {p, 45, 0},
        {p, 15, 0}
        // A total of 90 bytes.
    };
    const size_t num_write_args = sizeof(write_args) / sizeof(write_args[0]);

    pipe_test_arg_t read_args[5] = {
        {p, 10, 0},
        {p, 50, 0},
        {p, 5, 0},
        {p, 5, 0},
        {p, 20, 0}
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

    sc_handle_close(p);

    TEST_SUCCEED();
}

static void *test_interrupted_waits_worker(void *arg) {
    TEST_TRUE(arg != NULL);

    pipe_test_arg_t *pipe_arg = (pipe_test_arg_t *)arg;
    handle_t p = pipe_arg->p;
    uint32_t should_read = pipe_arg->arg0;

    // NOTE: This used to test for FOS_E_STATE_MISMATCH, but some threads will execute the wait
    // AFTER `p` has been closed. This would likely result in FOS_E_INVALID_INDEX.
    if (should_read) {
        TEST_FAILURE(sc_handle_wait_read_ready(p));
    } else {
        TEST_FAILURE(sc_handle_wait_write_ready(p));
    }

    return NULL;
}

static bool test_interrupted_waits(void) {

    // First try interrupting readers.

    handle_t p;
    TEST_SUCCESS(sc_pipe_open(&p, 100));

    pipe_test_arg_t read_arg = {
        p, 1, 0
    };

    const size_t num_readers = 10;

    for (size_t i = 0; i < num_readers; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_interrupted_waits_worker, &read_arg));
    }

    sc_thread_sleep(10);
    sc_handle_close(p);

    for (size_t i = 0; i < num_readers; i++) {
        TEST_SUCCESS(sc_thread_join(full_join_vector(), NULL, NULL));
    }

    // Next try interrupting writers.
    TEST_SUCCESS(sc_pipe_open(&p, 1));
    TEST_SUCCESS(sc_handle_write_full(p, "a", 1)); // pipe must be full for writers
                                                                // to block!

    pipe_test_arg_t write_arg = {
        p, 0, 0,
    };

    const size_t num_writers = 15;

    for (size_t i = 0; i < num_writers; i++) {
        TEST_SUCCESS(sc_thread_spawn(NULL, test_interrupted_waits_worker, &write_arg));
    }

    sc_thread_sleep(10);
    sc_handle_close(p);

    for (size_t i = 0; i < num_writers; i++) {
        TEST_SUCCESS(sc_thread_join(full_join_vector(), NULL, NULL));
    }

    TEST_SUCCEED();
}

bool test_syscall_pipe(void) {
    BEGIN_SUITE("Syscall Pipe");
    RUN_TEST(test_simple_rw0);
    RUN_TEST(test_multithread_rw0);
    RUN_TEST(test_multithread_rw1);
    RUN_TEST(test_interrupted_waits);
    return END_SUITE();
}
