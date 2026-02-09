
#include "u_startup/main.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_gfx.h"

#include "u_startup/test/syscall.h"
#include "u_startup/test/syscall_fs.h"
#include "u_startup/test/syscall_pipe.h"
#include "s_mem/simple_heap.h"
#include "s_util/constraints.h"

#include "u_startup/test/syscall_term.h"

proc_exit_status_t user_main(void) {

    /*
     * User Code.
     *
     * NOTE: This MR stips view of the VGA BIOS terminal. However, it does not give the user
     * access to the new graphics display buffer.
     *
     * The user has no ability to render anything to the screen at this commit.
     */

    fernos_error_t err;

    handle_t h_t;
    gfx_term_buffer_attrs_t attrs = {
        .palette = *BASIC_ANSI_PALETTE,
        .fmi = ASCII_MONO_8X16_FMI,
        .w_scale = 1, .h_scale = 1
    };

    err = sc_gfx_new_terminal(&h_t, &attrs);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    err = test_terminal_fork0(h_t);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_handle_close(h_t);

    return PROC_ES_SUCCESS;
}
