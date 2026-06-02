#include "u_startup/test/syscall_gfx.h"
#include "u_startup/syscall_gfx.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static bool wait_gfx_win_cleanup(handle_t h, gfx_color_t *(*bufs)[2]) {
    LOGF_PREFIXED("Please close window with handle 0x%X\n", h);

    window_event_t ev;
    do {
        TEST_SUCCESS(sc_gfx_wait_event(h));
        TEST_SUCCESS(sc_gfx_read_events(h, &ev, 1, NULL));
    } while (ev.event_code != WINEC_DEREGISTERED);

    sc_shm_close_shm((*bufs)[0]);
    sc_shm_close_shm((*bufs)[1]);
    sc_handle_close(h);

    TEST_SUCCEED();
}

static bool test_create_and_swap(void) {
    handle_t h;
    gfx_color_t *bufs[2];

    TEST_SUCCESS(sc_gfx_new_gfx_window(&h, &bufs));

    TEST_TRUE(wait_gfx_win_cleanup(h, &bufs));
    
    TEST_SUCCEED();
}

bool test_syscall_gfx(void) {
    BEGIN_SUITE("GFX Unit");
    test_create_and_swap();
    return END_SUITE();
}
