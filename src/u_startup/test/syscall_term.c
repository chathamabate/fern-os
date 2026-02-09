
#include "u_startup/test/syscall_term.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_term.h"
#include "s_util/ansi.h"
#include "s_util/misc.h"
#include "s_gfx/window.h"
#include "s_util/constraints.h"

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
    char log_buf[80];

    PROP_ERR(sc_proc_fork(&cpid));

    if (cpid == FOS_MAX_PROCS) {
        // Ok, the child process is going to work a little differently.
        // We are going to use a polling style!

        while (1) {
            // This is going to be written so that we can use the NULL
            // buffer poll method. (If this weren't a test, I'd do this differnetly)

            err = sc_term_read_events(h_t, NULL, 0, NULL);
            if (err != FOS_E_SUCCESS) {
                if (err != FOS_E_EMPTY) {
                    sc_proc_exit(PROC_ES_FAILURE);
                }

                sc_thread_sleep(1); 
                continue;
            }

            // Ok, now we basically just do the same thing again, realize the if statement above
            // doesn't gaurantee there will not be events.

            window_event_t ev;
            while ((err = sc_term_read_events(h_t, &ev, 1, NULL)) == FOS_E_SUCCESS) {
                if (ev.event_code == WINEC_DEREGISTERED) {
                    sc_proc_exit(PROC_ES_SUCCESS);
                }

                if (ev.event_code == WINEC_TICK) {
                    continue; // Won't even bother logging ticks here.
                }

                win_ev_to_str(log_buf, ev, true);
                err = sc_handle_write_fmt_s(h_t, ANSI_BLUE_FG "C:" ANSI_RESET " %s\n", log_buf);
                if (err != FOS_E_SUCCESS) {
                    sc_proc_exit(PROC_ES_FAILURE);
                }
            }

            if (err != FOS_E_EMPTY) {
                sc_proc_exit(PROC_ES_FAILURE);
            }
        }
    }

    // Ok, for the parent, we'll do the traditional event reading method.
    window_event_t ev_buf[5];
    while (true) {
        PROP_ERR(sc_term_wait_event(h_t));

        size_t evs_read;
        err = sc_term_read_events(h_t, ev_buf, sizeof(ev_buf) / sizeof(ev_buf[0]), &evs_read);
        if (err != FOS_E_SUCCESS) {
            if (err != FOS_E_EMPTY) {
                return err;
            }

            continue; // child process could've gotten there first!
        }

        for (size_t i = 0; i < evs_read; i++) {
            window_event_t ev = ev_buf[i];
            if (ev.event_code == WINEC_DEREGISTERED) {
                PROP_ERR(sc_signal_wait(1UL << FSIG_CHLD, NULL));

                proc_exit_status_t rces;
                PROP_ERR(sc_proc_reap(cpid, NULL, &rces));

                sc_signal_allow(old_sv);

                if (rces != PROC_ES_SUCCESS) {
                    return FOS_E_UNKNWON_ERROR;
                }

                return FOS_E_SUCCESS;
            }

            if (ev.event_code == WINEC_TICK) {
                continue;
            }
            
            win_ev_to_str(log_buf, ev, true);
            PROP_ERR(sc_handle_write_fmt_s(h_t, ANSI_RED_FG "P: " ANSI_RESET "%s\n", log_buf));
        }
    }
}
