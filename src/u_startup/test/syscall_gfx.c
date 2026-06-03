#include "u_startup/test/syscall_gfx.h"
#include "u_startup/syscall_gfx.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall.h"
#include "s_util/ps2_scancodes.h"
#include "s_util/constraints.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static bool wait_gfx_press_space(handle_t h, const char *msg) {
    LOGF_PREFIXED(msg);

    window_event_t ev;
    do {
        TEST_SUCCESS(sc_gfx_wait_event(h));
        TEST_SUCCESS(sc_gfx_read_events(h, &ev, 1, NULL));
    } while (!(ev.event_code == WINEC_KEY_INPUT && ev.d.key_code == SCS1_SPACE));

    TEST_SUCCEED();
}

static bool wait_gfx_win_cleanup(handle_t h, gfx_color_t *(*bufs)[2]) {
    LOGF_PREFIXED("Please close window with handle 0x%X\n", h);

    window_event_t ev;
    do {
        TEST_SUCCESS(sc_gfx_wait_event(h));
        TEST_SUCCESS(sc_gfx_read_events(h, &ev, 1, NULL));
    } while (ev.event_code != WINEC_DEREGISTERED);

    sc_shm_close_shm((*bufs)[1]);
    sc_shm_close_shm((*bufs)[0]);
    sc_handle_close(h);

    TEST_SUCCEED();
}

static bool test_create_and_swap(void) {
    handle_t h;
    gfx_color_t *bufs[2];

    TEST_SUCCESS(sc_gfx_new_gfx_window(&h, &bufs));

    size_t wid, hei;
    sc_gfx_get_dimmensions(h, &wid, &hei);

    mem_set(bufs[0], 0x0F, wid * hei * sizeof(gfx_color_t));
    mem_set(bufs[1], 0x3F, wid * hei * sizeof(gfx_color_t));

    for (size_t i = 0; i < 4; i++) {
        wait_gfx_press_space(h, "Press space to swap buffers\n");
        sc_gfx_swap(h);
    }

    TEST_TRUE(wait_gfx_win_cleanup(h, &bufs));
    
    TEST_SUCCEED();
}

static bool test_lasting_shms(void) {
    handle_t h;
    gfx_color_t *bufs[2];

    TEST_SUCCESS(sc_gfx_new_gfx_window(&h, &bufs));

    LOGF_PREFIXED("Please close window with handle 0x%X\n", h);

    window_event_t ev;
    do {
        TEST_SUCCESS(sc_gfx_wait_event(h));
        TEST_SUCCESS(sc_gfx_read_events(h, &ev, 1, NULL));
    } while (ev.event_code != WINEC_DEREGISTERED);

    sc_handle_close(h);

    // Here, even though the handle is closed, we should still be able to read/write to 
    // shared buffers!
    
    mem_set(bufs[0], 0x1F, sizeof(gfx_color_t) * FERNOS_GFX_WIDTH * FERNOS_GFX_HEIGHT);
    mem_set(bufs[1], 0x3F, sizeof(gfx_color_t) * FERNOS_GFX_WIDTH * FERNOS_GFX_HEIGHT);

    sc_shm_close_shm(bufs[1]);

    mem_set(bufs[0], 0x0F, sizeof(gfx_color_t) * FERNOS_GFX_WIDTH * FERNOS_GFX_HEIGHT);

    sc_shm_close_shm(bufs[0]);

    TEST_SUCCEED();
}

static bool test_gfx_fork(void) {
    // as a graphics window is composed of a handle state and shared memory regions, 
    // forking should cause no problems!

    sig_vector_t old_sv = sc_signal_allow(1 << FSIG_CHLD);

    handle_t h;
    gfx_color_t *bufs[2];
    TEST_SUCCESS(sc_gfx_new_gfx_window(&h, &bufs));

    proc_id_t cpids[5];
    for (size_t i = 0; i < sizeof(cpids) / sizeof(cpids[0]); i++) {
        TEST_SUCCESS(sc_proc_fork(cpids + i));

        if (cpids[i] == FOS_MAX_PROCS) { // Child process work!
            window_event_t ev;
            for (size_t j = 0; j < 5; j++) {
                TEST_SUCCESS(sc_gfx_read_event_single(h, &ev));

                mem_set(bufs[0] + (6 * FERNOS_GFX_WIDTH * i), 0xF + (j * 16), 6 * FERNOS_GFX_WIDTH * sizeof(gfx_color_t));
            }

            // By exiting handles and shms should be closed just fine!
            sc_proc_exit(PROC_ES_SUCCESS);
        }
    }

    LOGF_PREFIXED("Press some keys!\n");
    LOGF_PREFIXED("You should see lightening bands\n");

    for (size_t i = 0; i < sizeof(cpids) / sizeof(cpids[0]); i++) {
        proc_exit_status_t rces;
        TEST_SUCCESS(sc_proc_reap_single(cpids[i], NULL, &rces));
        TEST_EQUAL_HEX(PROC_ES_SUCCESS, rces);
    }

    TEST_TRUE(wait_gfx_win_cleanup(h, &bufs));
    sc_signal_allow(old_sv);

    TEST_SUCCEED();
}

bool test_syscall_gfx(void) {
    BEGIN_SUITE("GFX Unit");
    RUN_TEST(test_create_and_swap);
    RUN_TEST(test_lasting_shms);
    RUN_TEST(test_gfx_fork);
    return END_SUITE();
}
