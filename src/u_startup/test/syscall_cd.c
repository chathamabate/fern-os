
#include "u_startup/test/syscall_cd.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_cd.h"
#include "s_util/misc.h"
#include "s_util/ansi.h"

fernos_error_t test_syscall_cd_dimmensions(void) {
    handle_t cd;

    PROP_ERR(sc_vga_cd_open(&cd));

    size_t rows, cols;
    sc_cd_get_dimmensions(cd, &rows, &cols);

    // Ok now, let's make a cool border.
    char buf[32];

    PROP_ERR(sc_handle_write_s(cd, ANSI_CYAN_BG));

    // We'll start with the bottom row.
    PROP_ERR(sc_handle_write_s(cd, ansi_set_cursor_position(buf, rows, 1)));
    for (size_t i = 0; i < cols; i++) {
        PROP_ERR(sc_handle_write_s(cd, " "));
    }

    PROP_ERR(sc_handle_write_s(cd, ANSI_SCROLL_DOWN));

    PROP_ERR(sc_handle_write_s(cd, ansi_set_cursor_position(buf, 1, 1)));
    for (size_t i = 0; i < cols; i++) {
        PROP_ERR(sc_handle_write_s(cd, " "));
    }

    for (size_t i = 1; i < rows - 1; i++) {
        PROP_ERR(sc_handle_write_fmt_s(cd, " %s ", ansi_set_cursor_col(buf, cols)));
    }
    
    PROP_ERR(sc_handle_write_s(cd, ANSI_RESET));

    PROP_ERR(sc_handle_write_s(cd, ansi_set_cursor_position(buf, rows, cols)));

    // We should really always close, but since this is just a test,
    // We'll assume when an error occurs something more important is 
    // not working.
    sc_handle_close(cd);

    return FOS_E_SUCCESS;
}

fernos_error_t test_syscall_cd_big(void) {
    handle_t cd;

    PROP_ERR(sc_vga_cd_open(&cd));

    // This is likely larger than the max tx size. (The point of this test)
    char big_buf[5000];
    const size_t buf_len = (sizeof(big_buf) / sizeof(big_buf[0])) - 1; // Leave room for NT.

    const char *pieces[2] = {
        ANSI_GREEN_FG "Hello World Wo wowowowowo\n",
        ANSI_RED_FG "HELLO WORLD aye yo aye yo!!!!\n"
    };
    const size_t num_pieces = sizeof(pieces) / sizeof(pieces[0]);

    const size_t pieces_len[2] = {
        str_len(pieces[0]),
        str_len(pieces[1]),
    };

    size_t pos = 0;
    size_t iter = 0;

    while (true) {
        const size_t i = iter % num_pieces;
        
        if (pos + pieces_len[i] > buf_len) {
            break; // no more space, time to exit.
        }

        str_cpy(&(big_buf[pos]), pieces[i]);

        pos += pieces_len[i];
        iter++;
    }

    big_buf[pos] = '\0';

    // NOTE: we aren't using write full here!
    // We want to see if the message we are trying to write gets cut off near the max!
    // If so, are the ANSI codes still intact? (I.e. do all the colors look right)

    size_t written;
    PROP_ERR(sc_handle_write(cd, big_buf, pos, &written));

    PROP_ERR(sc_handle_write_s(cd, ANSI_RESET));

    PROP_ERR(sc_handle_write_fmt_s(cd, "\n%u / %u Written\n\n", written, pos));

    sc_handle_close(cd);

    return FOS_E_SUCCESS;
}
