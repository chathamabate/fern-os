
#include "u_startup/test/syscall_default_io.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_fs.h"

#include "s_bridge/shared_defs.h"
#include "s_util/constraints.h"


static handle_t results_cd;

#define LOGF_METHOD(...) sc_handle_write_fmt_s(results_cd, __VA_ARGS__)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

static handle_t in;
static handle_t out;

static bool pretest(void) {
    TEST_SUCCESS(sc_fs_touch("input"));
    TEST_SUCCESS(sc_fs_touch("output"));

    TEST_SUCCESS(sc_fs_open("input", &in));
    TEST_SUCCESS(sc_fs_open("output", &out));

    sc_set_in_handle(in);
    sc_set_out_handle(out);

    TEST_SUCCEED();
} 

static bool posttest(void) {
    sc_handle_close(out);
    sc_handle_close(in);

    TEST_SUCCESS(sc_fs_remove("output"));
    TEST_SUCCESS(sc_fs_remove("input"));

    TEST_SUCCEED();
}

static bool test_null_io_handles(void) {
    fernos_error_t err;

    sc_set_in_handle(FOS_MAX_HANDLES_PER_PROC + 100);

    TEST_EQUAL_UINT(FOS_MAX_HANDLES_PER_PROC, sc_get_in_handle());

    char buf[5];
    err = sc_in_read(buf, sizeof(buf), NULL);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    err = sc_in_wait();
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    sc_set_out_handle(FOS_MAX_HANDLES_PER_PROC);
    TEST_EQUAL_UINT(FOS_MAX_HANDLES_PER_PROC, sc_get_out_handle());

    size_t written;
    err = sc_out_write("hello", 5, &written);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_UINT(5, written);

    TEST_SUCCEED();
}

static bool test_valid_io_handles(void) {
    char buf[128];

    TEST_EQUAL_HEX(in, sc_get_in_handle());
    TEST_EQUAL_HEX(out, sc_get_out_handle());

    const char *msg = "hello";
    const size_t msg_len = str_len(msg);

    TEST_SUCCESS(sc_handle_write_s(in, msg));
    TEST_SUCCESS(sc_fs_seek(in, 0));

    TEST_SUCCESS(sc_in_read_full(buf, msg_len));
    buf[msg_len] = '\0';

    TEST_TRUE(str_eq(msg, buf));
    mem_set(buf, 0, sizeof(buf));

    TEST_SUCCESS(sc_out_write_s(msg));
    TEST_SUCCESS(sc_fs_seek(out, 0));

    TEST_SUCCESS(sc_handle_read_full(out, buf, msg_len));
    buf[msg_len] = '\0';
    TEST_TRUE(str_eq(msg, buf));

    TEST_SUCCEED();
}

static bool test_fork_io_handles(void) {
    // When forking, the spawned process should have the smae default input/output handle
    // indeces!

    sig_vector_t old_sv = sc_signal_allow(1 << FSIG_CHLD);

    proc_id_t cpid;
    TEST_SUCCESS(sc_proc_fork(&cpid));

    char buf[128];

    if (cpid == FOS_MAX_PROCS) { // child process!
        TEST_SUCCESS(sc_in_read_full(buf, 5)); // "Hello"
        buf[5] = '\0';

        TEST_TRUE(str_eq("Hello", buf));

        TEST_SUCCESS(sc_out_write_s("Aye"));

        sc_proc_exit(PROC_ES_SUCCESS);
    }

    TEST_SUCCESS(sc_handle_write_s(in, "Hello"));

    TEST_SUCCESS(sc_handle_read_full(out, buf, 3)); // "Aye"
    buf[3] = '\0';

    TEST_TRUE(str_eq("Aye", buf));

    TEST_SUCCESS(sc_signal_wait(1 << FSIG_CHLD, NULL));
    TEST_SUCCESS(sc_proc_reap(cpid, NULL, NULL));

    sc_signal_allow(old_sv);

    TEST_SUCCEED();
}

bool test_syscall_default_io(handle_t cd) {
    results_cd = cd;

    BEGIN_SUITE("Syscall Default IO");
    RUN_TEST(test_null_io_handles);
    RUN_TEST(test_valid_io_handles);
    RUN_TEST(test_fork_io_handles);
    return END_SUITE();
}
