
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
            const char *ev_name = WINEC_NAME_MAP[ev.event_code];

            switch (ev.event_code) {
            case WINEC_DEREGISTERED:
                return FOS_E_SUCCESS;

            case WINEC_RESIZED:
                PROP_ERR(sc_handle_write_fmt_s(h_t, "[%s] %u %u\n", 
                            ev_name, ev.d.dims.width, ev.d.dims.height));
                break;

            case WINEC_TICK:
                if (tick_num % 64 == 0) {
                    PROP_ERR(sc_handle_write_fmt_s(h_t, "[%s] %u\n",
                                ev_name, tick_num));
                }
                tick_num++;
                break; 

            case WINEC_KEY_INPUT:
                PROP_ERR(sc_handle_write_fmt_s(h_t, "[%s] 0x04%X\n",
                            ev_name, ev.d.key_code));
                break;

            default: 
                if (!ev_name) {
                    PROP_ERR(sc_handle_write_s(h_t, 
                                ANSI_BRIGHT_RED_FG "Unknown Event Received!\n" ANSI_RESET));
                    return FOS_E_UNKNWON_ERROR;
                }
                PROP_ERR(sc_handle_write_fmt_s(h_t, "[%s]\n",
                            ev_name));
                break;
            }
        }
    }
}

fernos_error_t test_terminal_fork0(handle_t h_t) {
    return FOS_E_NOT_IMPLEMENTED;
}
