
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "s_util/constraints.h"
#include "s_util/str.h"
#include "s_util/elf.h"
#include <stdarg.h>

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

/*
 * NOTE: These tests are not meant to rigorosly test reading a writing to files/building
 * complex file trees. These cases are already covered by the file system tests inside the kernel.
 *
 * These tests make sure that processes and threads behave as expected when reading/writing to
 * file concurrently.
 */

/**
 * Each test will start in an empty instance of this directory. 
 * At the end of each test, this directory will be deleted.
 */
#define _TEMP_TEST_DIR_PATH "syscall_fs_test"
#define TEMP_TEST_DIR_PATH "/" _TEMP_TEST_DIR_PATH 

static sig_vector_t old_sv;
static size_t old_user_al;

static bool pretest(void) {
    TEST_TRUE(get_default_allocator() != NULL);

    old_user_al = al_num_user_blocks(get_default_allocator());

    fernos_error_t err;

    // Since we will be working with child processes, we will allow the 
    // child signal in root.
    old_sv = sc_signal_allow(1 << FSIG_CHLD);

    err = sc_fs_mkdir(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_set_wd(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static bool posttest(void) {
    fernos_error_t err;

    err = sc_fs_set_wd("/");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_flush_all();
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    sc_signal_allow(old_sv);

    size_t new_user_al = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(old_user_al, new_user_al);

    TEST_SUCCEED();
}

static bool test_simple_rw(void) {
    fernos_error_t err;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t fh;

    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_FALSE(sc_handle_is_cd(fh));

    const char *msg = "Hello FS";
    const size_t msg_size = str_len(msg) + 1;

    // A file should always be able to accept data (Unless full)
    err = sc_handle_wait_write_ready(fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_handle_write_full(fh, msg, msg_size);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    fs_node_info_t info;
    err = sc_fs_get_info("./a.txt", &info);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_UINT(msg_size, info.len);

    err = sc_fs_seek(fh, 0);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    char rx_buf[50];
    err = sc_handle_read_full(fh, rx_buf, msg_size);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    
    TEST_TRUE(str_eq(msg, rx_buf));

    // When there isn't more to read, this should return FOS_E_EMPTY without blocking.
    err = sc_handle_read(fh, rx_buf, msg_size, NULL);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    // This confirms NULL is ok for the written pointer argument.
    err = sc_handle_write(fh, msg, msg_size, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_IN_USE, err);

    sc_handle_close(fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_multithread_rw_worker(void *arg) {
    (void)arg;

    fernos_error_t err;

    handle_t fh;

    err = sc_fs_open("./b.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    uint8_t rx_buf[10];

    for (size_t i = 0; i < 10; i++) {
        err = sc_handle_read_full(fh, rx_buf, sizeof(rx_buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        for (size_t j = 0; j < sizeof(rx_buf); j++) {
            TEST_EQUAL_UINT(i, rx_buf[j]);
        }
    }

    sc_handle_close(fh);

    return NULL;
}

static bool test_multithread_rw(void) {
    fernos_error_t err;

    err = sc_fs_touch("./b.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_thread_spawn(NULL, test_multithread_rw_worker, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t fh;

    err = sc_fs_open("./b.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    uint8_t tx_buf[10];

    for (size_t i = 0; i < 10; i++) {
        mem_set(tx_buf, (uint8_t)i, sizeof(tx_buf));

        err = sc_handle_write_full(fh, tx_buf, sizeof(tx_buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        sc_thread_sleep(1);
    }

    sc_handle_close(fh);

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("./b.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    
    TEST_SUCCEED();
}

static void *test_multithread_wait_same_handle_worker(void *arg) {
    handle_t h = *(handle_t *)arg;

    TEST_SUCCESS(sc_handle_wait_read_ready(h));

    return NULL;
}

static bool test_multithread_wait_same_handle(void) {
    // Multiple threads ARE allowed to wait on the same handle at the same time.
    // (However, it's really not advised)
    // Reading at the same time could cause a system shutdown.

    thread_id_t workers[5];
    const size_t num_workers = sizeof(workers) / sizeof(workers[0]);

    TEST_SUCCESS(sc_fs_touch("./a.txt"));

    handle_t read_h;
    TEST_SUCCESS(sc_fs_open("./a.txt", &read_h));


    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_spawn(workers + i, test_multithread_wait_same_handle_worker, &read_h));
    }

    handle_t write_h;
    TEST_SUCCESS(sc_fs_open("./a.txt", &write_h));
    TEST_SUCCESS(sc_handle_write_full(write_h, "a", 1));

    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_join(1 << workers[i], NULL, NULL));
    }

    sc_handle_close(write_h);
    sc_handle_close(read_h);
    TEST_SUCCESS(sc_fs_remove("./a.txt"));

    TEST_SUCCEED();
}

static void *test_multithread_multihandle_worker(void *arg) {
    handle_t h = *(handle_t *)arg;

    char msg[6];
    TEST_SUCCESS(sc_handle_read_full(h, msg, sizeof(msg)));
    TEST_TRUE(str_eq("hello", msg));

    return NULL;
}

static bool test_multithread_multihandle(void) {
    // Multiple threads should all be able to read from the same file at the same time GIVEN
    // they are using different handles.

    thread_id_t workers[10];
    handle_t read_handles[sizeof(workers) / sizeof(workers[0])];
    const size_t num_workers = sizeof(workers) / sizeof(workers[0]);

    TEST_SUCCESS(sc_fs_touch("./a.txt"));

    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_fs_open("./a.txt", read_handles + i));
        TEST_SUCCESS(sc_thread_spawn(workers + i, test_multithread_multihandle_worker, read_handles + i));
    }

    // We want at least 2 of the workers to be waiting before we write.
    sc_thread_sleep(1);

    handle_t write_handle;
    TEST_SUCCESS(sc_fs_open("./a.txt", &write_handle));
    TEST_SUCCESS(sc_handle_write_full(write_handle, "hello", 6));

    for (size_t i = 0; i < num_workers; i++) {
        TEST_SUCCESS(sc_thread_join(1 << workers[i], NULL, NULL));
    }

    sc_handle_close(write_handle);

    for (size_t i = 0; i < num_workers; i++) {
        sc_handle_close(read_handles[i]);
    }

    TEST_SUCCESS(sc_fs_remove("./a.txt"));

    TEST_SUCCEED();
}

static bool test_multiprocess_rw(void) {
    fernos_error_t err;

    // Pretty simple test, write from parent, read from child.

    // Trying an absolute path because what the hell.
    err = sc_fs_touch(TEMP_TEST_DIR_PATH "/a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t fh;

    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    uint8_t buf[100];

    mem_set(buf, 5, sizeof(buf));

    // Let's start by advancing the position of the file handle to confirm
    // the file handle copy works as expected!

    err = sc_handle_write_full(fh, buf, sizeof(buf));
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) { // Child process!
        for (size_t i = 0; i < 10; i++) {
            err = sc_handle_read_full(fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            for (size_t j = 0; j < sizeof(buf); j++) {
                TEST_EQUAL_UINT(i, buf[j]);
            }
        }

        // We will exit WITHOUT closing our file handle. This is to test that
        // the file handle is closed automatically during a reap.
        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // Parent process!
    for (size_t i = 0; i < 10; i++) {
        mem_set(buf, i, sizeof(buf));

        err = sc_handle_write_full(fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        sc_thread_sleep(1);
    }

    err = sc_signal_wait(1 <<  FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    sc_handle_close(fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_multithread_and_process_rw_worker(void *arg) {
    (void)arg;

    fernos_error_t err;

    handle_t b_fh;
    err = sc_fs_open("./b.txt", &b_fh);

    uint8_t buf[100];

    for (size_t i = 0; i < 10; i++) {
        err = sc_handle_read_full(b_fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        for (size_t j = 0; j < sizeof(buf); j++) {
            TEST_EQUAL_UINT(i + 1, buf[j]);
        }
    }

    sc_handle_close(b_fh);

    return NULL;
}

static bool test_multithread_and_process_rw(void) {
    //  (root proc)           (child proc)
    //     worker0  --> "a.txt"  ---
    //                             |
    //     worker1  <-- "b.txt"  <--

    fernos_error_t err;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_touch("./b.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t a_fh;

    uint8_t buf[100];

    err = sc_fs_open("./a.txt", &a_fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    proc_id_t cpid;
    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    if (cpid == FOS_MAX_PROCS) { // Child process
        handle_t b_fh;

        err = sc_fs_open("./b.txt", &b_fh);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        for (size_t i = 0; i < 10; i++) {
            err = sc_handle_read_full(a_fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            for (size_t j = 0; j < sizeof(buf); j++) {
                buf[j]++;
            }

            err = sc_handle_write_full(b_fh, buf, sizeof(buf));
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        }

        sc_handle_close(a_fh);
        sc_handle_close(b_fh);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // Parent process.
    
    err = sc_thread_spawn(NULL, test_multithread_and_process_rw_worker, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (size_t i = 0; i < 10; i++) {
        mem_set(buf, i, sizeof(buf));

        err = sc_handle_write_full(a_fh, buf, sizeof(buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    err = sc_signal_wait(1 << FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Wait for middle man process to exit before joining on worker thread.

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    sc_handle_close(a_fh);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("./b.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static void *test_early_close_worker(void *arg) {
    handle_t fh = (handle_t)arg;

    uint32_t i;

    fernos_error_t err;

    err = sc_handle_read_full(fh, &i, sizeof(i));
    TEST_EQUAL_HEX(FOS_E_STATE_MISMATCH, err);

    // Trying to read from a handle which doesn't exist!
    err = sc_handle_read_full(fh, &i, sizeof(i));
    TEST_TRUE(err != FOS_E_SUCCESS);

    return NULL;
}

static bool test_early_close(void) {
    fernos_error_t err;

    // Here we test that closing all handles before a read is done causes an
    // early wakeup! (With FOS_E_STATE_MISMATCH as the return)

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t fh;
    err = sc_fs_open("./a.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_thread_spawn(NULL, test_early_close_worker, (void *)fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Ok, this isn't really the best made test, we are assuming that by the time
    // time we wake up, the worker will have started to wait on data over `fh`.
    // This might not always be gauranteed. However, the read call will ALWAYS fail.
    sc_thread_sleep(5);

    // Close the very handle the worker is reading from :(
    sc_handle_close(fh);

    err = sc_thread_join(full_join_vector(), NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_many_procs(void) {
    const char *pipe_names[] = {
        "a", "b", "c", "d", "e", "f", "g"
    };
    const size_t num_pipes = sizeof(pipe_names) / sizeof(pipe_names[0]);

    fernos_error_t err;
    for (size_t i = 0; i < num_pipes; i++) {
        err = sc_fs_touch(pipe_names[i]);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Ok, we are going to have one child process for every pipe.
    // Root - pipe[0] -> proc1 - pipe[1] -> proc2 ... - pipe[num_pipes - 1] -> proc num_pipes 
    
    size_t proc_number;
    proc_id_t cpid;

    handle_t h;

    char tx_buf[100];

    const char *msg = "HELLO";
    str_cpy(tx_buf, msg);
    const size_t tx_amt = str_len(msg) + 1;

    for (proc_number = 0; proc_number < num_pipes; proc_number++) {
        err = sc_fs_open(pipe_names[proc_number], &h);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        err = sc_proc_fork(&cpid);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        if (cpid != FOS_MAX_PROCS) { // Parent process.
            // Write in full.
            err = sc_handle_write_full(h, tx_buf, tx_amt);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            // Don't need this guy anymore.
            sc_handle_close(h);

            if (proc_number == 0) {
                break;
            } else {
                sc_proc_exit(PROC_ES_SUCCESS);
            }
        } else { // Child process.
            err = sc_handle_read_full(h, tx_buf, tx_amt);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            sc_handle_close(h);
            
            TEST_TRUE(str_eq(msg, tx_buf));
        }
    }

    if (proc_number == num_pipes) {
        // Exit the final child process out here.
        sc_proc_exit(PROC_ES_SUCCESS);
    }

    // Wait for all children to exit.
    size_t reaped = 0;
    while (true) {
        while ((err = sc_proc_reap(FOS_MAX_PROCS, NULL, NULL)) == FOS_E_SUCCESS) {
            reaped++;
        }
        TEST_EQUAL_HEX(FOS_E_EMPTY, err);

        if (reaped == num_pipes) {
            // We've reaped all children, so we know no more FSIG_CHLD's will be received!
            sc_signal_clear(1 << FSIG_CHLD);
            break;
        }

        err = sc_signal_wait(1 << FSIG_CHLD, NULL);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Finally, delete all pipes.
    for (size_t i = 0; i < num_pipes; i++) {
        err = sc_fs_remove(pipe_names[i]);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    TEST_SUCCEED();
}

static bool test_many_handles(void) {
    // Test pushing the handle limit.

    fernos_error_t err;

    handle_t handles[FOS_MAX_HANDLES_PER_PROC];
    size_t num_handles = 0;

    err = sc_fs_touch("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (num_handles = 0; num_handles < FOS_MAX_HANDLES_PER_PROC; num_handles++) {
        err = sc_fs_open("./a.txt", handles + num_handles);

        // This test will expect that the calling process may not have an empty
        // handle table. Thus, we'll create as many handles as we can until error.
        if (err != FOS_E_SUCCESS) {
            break;
        }
    }

    proc_id_t cpid;

    err = sc_proc_fork(&cpid);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // We are going to do this in BOTH PROCESSES!
    handle_t bad_handle;
    err = sc_fs_open("./a.txt", &bad_handle);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    uint8_t buf[FOS_MAX_HANDLES_PER_PROC];

    if (cpid == FOS_MAX_PROCS) {
        // Alright this is going to be a bit tricky!
        // Remember, that all handles have their OWN positions!!!
        for (size_t i = 0; i < num_handles; i++) {
            mem_set(buf, i, i + 1);

            err = sc_handle_write_full(handles[i], buf, i + 1);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            sc_handle_close(handles[i]);
        }
        
        // This confirms that all of our handles started at position 0.

        fs_node_info_t info;
        err = sc_fs_get_info("./a.txt", &info);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        TEST_EQUAL_UINT(num_handles, info.len);

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    err = sc_handle_read_full(handles[0], buf, num_handles);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Since we read the full buffer each time, after the second read, we can gaurantee
    // we are reading the entire file in its final state.
    
    err = sc_handle_read_full(handles[1], buf, num_handles);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (size_t i = 0 ; i < num_handles; i++) {
        TEST_EQUAL_UINT(num_handles - 1, buf[i]);
    }

    for (size_t i = 0; i < num_handles; i++) {
        sc_handle_close(handles[i]);
    }

    // There should be space at the very end!
    err = sc_fs_open("./a.txt", handles + 0);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    sc_handle_close(handles[0]);

    err = sc_signal_wait(1 << FSIG_CHLD, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_proc_reap(cpid, NULL, NULL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("./a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_dir_functions(void) {
    // Test functions like mkdir, set_wd, and get child name.
    // Pretty simple test tbh.

    fernos_error_t err;

    err = sc_fs_mkdir("./other_dir");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    char name_bufs[2][FS_MAX_FILENAME_LEN + 1];
    
    err = sc_fs_get_child_name(".", 0, name_bufs[0]);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_get_child_name(".", 1, name_bufs[1]);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_TRUE(!str_eq(name_bufs[0], name_bufs[1]));
    for (size_t i = 0; i < 2; i++) {
        TEST_TRUE(str_eq("a.txt", name_bufs[i]) || str_eq("other_dir", name_bufs[i]));
    }

    err = sc_fs_get_child_name(".", 2, name_bufs[0]);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_TRUE(name_bufs[0][0] == '\0');

    err = sc_fs_remove("a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_set_wd("..");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove(_TEMP_TEST_DIR_PATH "/other_dir");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_set_wd(_TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_big_file(void) {
    // This test is almost identical to an earlier test in this file, except
    // it writes and reads a 1MB file. (Which it actually does surprisingly fast imo)

    fernos_error_t err;

    err = sc_fs_mkdir("b");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_touch("b/a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    handle_t fh;

    err = sc_fs_open("b/a.txt", &fh);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    
    uint8_t big_buf[10097];
    for (size_t i = 0; i < 100; i++) {
        mem_set(big_buf, (uint8_t)i, sizeof(big_buf));

        err = sc_handle_write_full(fh, big_buf, sizeof(big_buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    err = sc_fs_seek(fh, 0);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (size_t i = 0; i < 100; i++) {
        err = sc_handle_read_full(fh, big_buf, sizeof(big_buf));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        for (size_t j = 0; j < sizeof(big_buf); j++) {
            TEST_EQUAL_UINT(i, big_buf[j]);
        }
    }

    sc_handle_close(fh);

    err = sc_fs_remove("b/a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = sc_fs_remove("b");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

static bool test_bad_fs_calls(void) {
    fernos_error_t err;

    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Can't set working directory to a file.
    err = sc_fs_set_wd("a.txt");
    TEST_EQUAL_HEX(FOS_E_STATE_MISMATCH, err);

    // Can't set working directory to a directory which doesn't exist.
    err = sc_fs_set_wd("./fake_dir");
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    // Trying to create a file which doesn't exist.
    err = sc_fs_touch("a.txt");
    TEST_EQUAL_HEX(FOS_E_IN_USE, err);

    // This path doesn't exist.
    err = sc_fs_touch("b/a.txt");
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    // Same outcomes expected for mkdir.
    err = sc_fs_mkdir("a.txt");
    TEST_EQUAL_HEX(FOS_E_IN_USE, err);

    err = sc_fs_mkdir("b/a.txt");
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    // Can't delete a non-empty directory.
    err = sc_fs_remove(TEMP_TEST_DIR_PATH);
    TEST_EQUAL_HEX(FOS_E_IN_USE, err);

    // Can't delete a file which doesn't exist.
    err = sc_fs_remove("c.txt");
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    // OK Now for get info/get childname
    fs_node_info_t info;

    err = sc_fs_get_info("b.txt", &info);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    char child_name[FS_MAX_FILENAME_LEN + 1];
    err = sc_fs_get_child_name("b", 0, child_name);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    err = sc_fs_get_child_name("a.txt", 0, child_name);
    TEST_EQUAL_HEX(FOS_E_STATE_MISMATCH, err);

    // Open and close.

    handle_t fh;

    err = sc_fs_open("b", &fh);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    err = sc_fs_open(".", &fh);
    TEST_EQUAL_HEX(FOS_E_STATE_MISMATCH, err);

    // Some random invalid file handle shouldn't cause issues.
    sc_handle_close((handle_t)238438);

    // Seek/read/write
    
    err = sc_fs_seek((handle_t)123, 3);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    size_t txed;
    uint32_t dummy;

    err = sc_handle_write((handle_t)3432, &dummy, sizeof(dummy), &txed);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    err = sc_handle_read((handle_t)3432, &dummy, sizeof(dummy), &txed);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    err = sc_fs_flush((handle_t)3432);
    TEST_EQUAL_HEX(FOS_E_INVALID_INDEX, err);

    err = sc_fs_remove("a.txt");
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_SUCCEED();
}

/*
 * ELF parser testing. Consider moving this to another file?
 */

#define TEST_ELF_FILENAME "test.elf"

/**
 * Helper for `test_elf32_parsing`.
 */
static bool test_bad_elf32_file(const void *bad_elf_data, size_t elf_len) {
    TEST_SUCCESS(sc_fs_touch(TEST_ELF_FILENAME));

    handle_t fh;

    // Write out test elf data.
    TEST_SUCCESS(sc_fs_open(TEST_ELF_FILENAME, &fh));
    TEST_SUCCESS(sc_handle_write_full(fh, bad_elf_data, elf_len));
    sc_handle_close(fh);

    // Now try to parse, this should fail!

    user_app_t *ua;
    TEST_FAILURE(sc_fs_parse_elf32(get_default_allocator(), TEST_ELF_FILENAME, &ua));

    TEST_SUCCESS(sc_fs_remove(TEST_ELF_FILENAME));

    TEST_SUCCEED();
}

static bool test_elf32_parsing(void) {
    // Ok, for this test I am going to setup a "real" elf file in memory.
    // I will confirm that this parses successfully.
    //
    // Afterwards, I will screw around with the elf file and confirm failures!

    void * const dummy_entry = (void *)0x555U;
    const size_t num_pg_headers = 5; 
    const size_t bin_area_offset = sizeof(elf32_header_t) +
        (num_pg_headers * sizeof(elf32_program_header_t));
    const size_t bin_area_size = 0x1000U;
    const size_t elf_size = bin_area_offset + bin_area_size;

    uint8_t *elf_data = da_malloc(
        elf_size
    );

    TEST_TRUE(elf_data != NULL);

    elf32_header_t * const elf_header = (elf32_header_t *)elf_data;
    
    *elf_header = (elf32_header_t) {
        .header_magic = ELF_HEADER_MAGIC,
        .cls = 1, .endian = 1, .version0 = 1, .os_abi = 0xFF,
        .os_abi_version = 0, .pad0 = { 0 }, .obj_type = 0, 
        .machine = 0x03,
        .version1 = 1,
        .entry = dummy_entry, 
        .program_header_table = sizeof(elf32_header_t),
        .section_header_table = elf_size, // Not being used
        .flags = 0,
        .this_header_size = sizeof(elf32_header_t),
        .program_header_size = sizeof(elf32_program_header_t),
        .num_program_headers = num_pg_headers,
        .section_header_size = sizeof(elf32_section_header_t),
        .num_section_headers = 0,
        .section_names_header_index = 0,
    };

    elf32_program_header_t *pg_headers = (elf32_program_header_t *)((elf32_header_t *)elf_data + 1);
    
    // Maybe 3 program non-null program headers?

    uint8_t * const seg0_area = (uint8_t *)elf_data + bin_area_offset;
    const size_t seg0_size_in_file = 0x100U;

    mem_set(seg0_area, 0xAB, seg0_size_in_file);

    pg_headers[0] = (elf32_program_header_t) {
        .type = ELF32_SEG_TYPE_LOADABLE,
        .offset = (uintptr_t)seg0_area - (uintptr_t)elf_data,
        .vaddr = (void *)0x2000U,
        .paddr = 0,
        .size_in_file = seg0_size_in_file, 
        .size_in_mem =  0x1000U,
        .flags = ELF32_SEG_FLAG_READABLE | ELF32_SEG_FLAG_WRITEABLE,
        .align = 2
    };

    pg_headers[1] = (elf32_program_header_t) {
        .type = ELF32_SEG_TYPE_DYNAMIC, // Should be ignored by the parser.
    };

    uint8_t * const seg2_area = (uint8_t *)elf_data + bin_area_offset + 0x200U;
    const size_t seg2_size_in_file = 0x200U;

    mem_set(seg2_area, 0xCD, seg2_size_in_file);

    pg_headers[2] = (elf32_program_header_t) {
        .type = ELF32_SEG_TYPE_LOADABLE,
        .offset = (uintptr_t)seg2_area - (uintptr_t)elf_data,
        .vaddr = (void *)0x1000U,
        .paddr = 0,
        .size_in_file = seg2_size_in_file, 
        .size_in_mem =  0x200U,
        .flags = ELF32_SEG_FLAG_READABLE | ELF32_SEG_FLAG_EXECUTABLE,
        .align = 2
    };

    pg_headers[3] = (elf32_program_header_t) {
        .type = ELF32_SEG_TYPE_LOADABLE,
        .offset = 0,
        .vaddr = (void *)0x3000U,
        .paddr = 0,
        .size_in_file = 0,  // No file component!
        .size_in_mem =  0x200U,
        .flags = ELF32_SEG_FLAG_READABLE | ELF32_SEG_FLAG_WRITEABLE,
        .align = 2
    };

    // Unused entries.
    pg_headers[4] = (elf32_program_header_t) {
        .type = ELF32_SEG_TYPE_NULL
    };

    // Ok, now let's write out the ELF Files, and confirm it parses!
    
    TEST_SUCCESS(sc_fs_touch(TEST_ELF_FILENAME));

    handle_t fh;
    TEST_SUCCESS(sc_fs_open(TEST_ELF_FILENAME, &fh));
    TEST_SUCCESS(sc_handle_write_full(fh, elf_data, elf_size));
    sc_handle_close(fh);

    user_app_t *ua;
    TEST_SUCCESS(sc_fs_parse_elf32(get_default_allocator(), TEST_ELF_FILENAME, &ua));
    TEST_TRUE(ua != NULL);

    // Remove the test file since we don't need it anymore!
    TEST_SUCCESS(sc_fs_remove(TEST_ELF_FILENAME));

    // Let's confirm our user app looks right!
    TEST_EQUAL_HEX(dummy_entry, ua->entry);

    const size_t loaded_hdr_inds[] = {
        0, 2, 3 // The headers which should've been loaded into
                            // the resulting user app.
    };
    const size_t num_loaded_hdr_inds = sizeof(loaded_hdr_inds) / sizeof(loaded_hdr_inds[0]);

    for (size_t i = 0; i < num_loaded_hdr_inds; i++) {
        const size_t hdr_ind = loaded_hdr_inds[i];
        TEST_TRUE(ua->areas[i].occupied);
        if (pg_headers[hdr_ind].flags & ELF32_SEG_FLAG_WRITEABLE) {
            TEST_TRUE(ua->areas[i].writeable);
        }
        TEST_EQUAL_HEX(pg_headers[hdr_ind].vaddr, ua->areas[i].load_position);
        TEST_EQUAL_HEX(pg_headers[hdr_ind].size_in_file, ua->areas[i].given_size);
        TEST_EQUAL_HEX(pg_headers[hdr_ind].size_in_mem, ua->areas[i].area_size);

        if (pg_headers[hdr_ind].size_in_file > 0) {
            TEST_TRUE(ua->areas[i].given != NULL);
        }
    }

    TEST_TRUE(mem_cmp(ua->areas[0].given, seg0_area, seg0_size_in_file));
    TEST_TRUE(mem_cmp(ua->areas[1].given, seg2_area, seg2_size_in_file));

    // Confirm unoccupied areas!
    for (size_t i = num_loaded_hdr_inds; i < FOS_MAX_APP_AREAS; i++) {
        TEST_FALSE(ua->areas[i].occupied);
    }

    delete_user_app(ua);

    // Ok, finally, now onto failure cases!

    // Lengths too short.
    TEST_TRUE(test_bad_elf32_file(elf_data, 1));
    TEST_TRUE(test_bad_elf32_file(elf_data, sizeof(elf32_header_t)));
    TEST_TRUE(test_bad_elf32_file(elf_data, sizeof(elf32_header_t) 
                + ((num_pg_headers - 1) * sizeof(elf32_program_header_t))));
    TEST_TRUE(test_bad_elf32_file(elf_data, sizeof(elf32_header_t) 
                + (num_pg_headers * sizeof(elf32_program_header_t))));

    // Bad magic value
    elf_header->header_magic++;
    TEST_TRUE(test_bad_elf32_file(elf_data, elf_size));
    elf_header->header_magic--;

    // Too many program headers!
    elf_header->num_program_headers = 10000;
    TEST_TRUE(test_bad_elf32_file(elf_data, elf_size));
    elf_header->num_program_headers = num_pg_headers;

    // Alright, I could always add more cases, but I think we get the point.

    da_free(elf_data);

    TEST_SUCCEED();
}

bool test_syscall_fs(void) {
    BEGIN_SUITE("FS Syscalls");
    RUN_TEST(test_simple_rw);
    RUN_TEST(test_multithread_rw);
    RUN_TEST(test_multithread_wait_same_handle);
    RUN_TEST(test_multithread_multihandle);
    RUN_TEST(test_multiprocess_rw);
    RUN_TEST(test_multithread_and_process_rw);
    RUN_TEST(test_early_close);
    RUN_TEST(test_many_procs);
    RUN_TEST(test_many_handles);
    RUN_TEST(test_dir_functions);
    RUN_TEST(test_big_file);
    RUN_TEST(test_bad_fs_calls);
    RUN_TEST(test_elf32_parsing);
    return END_SUITE();
}
