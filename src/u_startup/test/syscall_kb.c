
#include "u_startup/test/syscall_kb.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_kb.h"
#include "s_util/misc.h"
#include "s_util/ps2_scancodes.h"
#include "s_util/ansi.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_cd.h"

fernos_error_t test_kb_simple_prompt(void) {
    // Again, this is just a test program, it only cleans up the keybaord handle 
    // during a successful run.

    handle_t kb;
    PROP_ERR(sc_kb_open(&kb));

    char msg_buf[60];
    const size_t msg_buf_len = sizeof(msg_buf) - 1; // Include 1 char for the NT.

    while (true) {
        PROP_ERR(sc_out_write_s(ANSI_BRIGHT_BLUE_FG ">" ANSI_RESET " "));

        size_t i = 0;
        scs1_code_t sc;
        bool lshift = false;
        bool rshift = false;

        while (i < msg_buf_len) {
            PROP_ERR(sc_handle_read_full(kb, &sc, sizeof(sc)));
            
            if (sc == SCS1_ESC) {
                sc_handle_close(kb);
                return FOS_E_SUCCESS;
            }

            if (sc == SCS1_ENTER) {
                break;
            }

            bool make = scs1_is_make(sc);
            sc = scs1_as_make(sc);

            switch (sc) {
            case SCS1_LSHIFT:
                lshift = make;
                break;

            case SCS1_RSHIFT:
                rshift = make;
                break;

            case SCS1_BACKSPACE:
                if (make && i > 0) {
                    PROP_ERR(sc_out_write_s(ANSI_CURSOR_LEFT " " ANSI_CURSOR_LEFT)); 
                    i--;
                }
                break;

            default:
                if (make) {
                    char out = lshift || rshift ? scs1_to_ascii_uc(sc) : scs1_to_ascii_lc(sc);
                    if (out) {
                        msg_buf[i++] = out;
                        PROP_ERR(sc_out_write(&out, sizeof(out), NULL));
                    }
                }
                break;
            }
        }

        msg_buf[i] = '\0';

        if (msg_buf[0] != '\0') {
            PROP_ERR(sc_out_write_fmt_s("\n%s\n", msg_buf));
        } else {
            PROP_ERR(sc_out_write_s("\n"));
        }
    }
}

fernos_error_t test_kb_funky(void) {
    handle_t kb;
    PROP_ERR(sc_kb_open(&kb));

    uint8_t data_buf[10];

    while (true) {
        PROP_ERR(sc_out_write_s("Reading...\n"));
        PROP_ERR(sc_handle_read_full(kb, data_buf, sizeof(data_buf) / 2));

        PROP_ERR(sc_out_write_s("Reading MORE...\n"));
        PROP_ERR(sc_handle_read_full(kb, data_buf + (sizeof(data_buf) / 2), sizeof(data_buf) / 2));

        PROP_ERR(sc_out_write_s("Enough Data Received!\n"));
        for (size_t i = 0; i < sizeof(data_buf); i++) {
            PROP_ERR(sc_out_write_fmt_s("%X ", data_buf[i]));
        }

        PROP_ERR(sc_out_write_s("\nSleeping (Ignoring presses)\n"));
        sc_thread_sleep(48);
        sc_kb_skip_forward(kb); // After the sleep we skip ahead.
    }
}
