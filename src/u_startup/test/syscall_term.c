
#include "u_startup/test/syscall_term.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_term.h"
#include "s_util/ansi.h"
#include "s_util/misc.h"
#include "s_gfx/window.h"

fernos_error_t test_userspace_dummy_term(handle_t h_t) {
    // Number of the next tick to be received.
    uint32_t tick_num = 0;

    size_t rows;
    size_t cols;

    char buf[100];

    // How should we do this?
    
    sc_term_get_dimmensions(h_t, &rows, &cols);
    PROP_ERR(sc_handle_write_s(h_t, ANSI_CLEAR_DISPLAY));

    while (true) {
        PROP_ERR(sc_term_wait_event(h_t));

        size_t num_evs;
        window_event_t evs[10];

        PROP_ERR(sc_term_read_events(h_t, evs, sizeof(evs) / sizeof(evs[0]), 
                    &num_evs));

        for (size_t i = 0; i < num_evs; i++) {
            window_event_t ev = evs[i];

            if (ev.event_code == WINEC_DEREGISTERED) {
                return FOS_E_SUCCESS;
            }

            if (ev.event_code == WINEC_TICK && (tick_num++ % 64)) {
                continue;
            }

            win_ev_to_str(buf, ev, true); 
            PROP_ERR(sc_handle_write_fmt_s(h_t, "%s\n", buf));
        }
    }
}

fernos_error_t test_terminal_fork0(handle_t h_t) {

    sig_vector_t old_sv = sc_signal_allow(1UL << FSIG_CHLD);

    fernos_error_t err;
    proc_id_t cpid;

    PROP_ERR(sc_proc_fork(&cpid));




    sc_signal_allow(old_sv);

    return FOS_E_NOT_IMPLEMENTED;
}
