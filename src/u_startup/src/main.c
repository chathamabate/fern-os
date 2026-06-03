
#include "u_startup/main.h"

#include "s_gfx/mono_fonts.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_gfx.h"
#include "u_startup/test/syscall_gfx.h"
#include "u_startup/syscall_term.h"
#include "u_startup/test/syscall_term.h"
#include "u_startup/test/syscall_kb.h"
#include "u_startup/test/syscall.h"

proc_exit_status_t user_main(void) {
    /*
     * User Code Here.
     * Ehhh, I don't really want to even do this shit rn...
     * I think I may take a lil nap nap tbh.
     */

    handle_t out;
    gfx_term_buffer_attrs_t attrs = {
        .fmi = ASCII_MONO_8X16_FMI,
        .w_scale = 1, .h_scale = 1,
        .palette = *BASIC_ANSI_PALETTE
    };
    sc_gfx_new_terminal(&out, &attrs);
    sc_set_out_handle(out);

    test_syscall_gfx();

    while (1);

    return PROC_ES_SUCCESS;
}
