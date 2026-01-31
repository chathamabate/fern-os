
#include "u_startup/main.h"
#include "u_startup/syscall_gfx.h"

proc_exit_status_t user_main(void) {

    /*
     * User Code.
     *
     * NOTE: This MR stips view of the VGA BIOS terminal. However, it does not give the user
     * access to the new graphics display buffer.
     *
     * The user has no ability to render anything to the screen at this commit.
     */
    sc_gfx_new_dummy();
    while (1);

    return PROC_ES_SUCCESS;
}
