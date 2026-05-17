
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_gfx.h"
#include "u_startup/test/syscall.h"


proc_exit_status_t user_main(void) {
    /*
     * User Code Here.
     */

    handle_t h;
    sc_gfx_new_terminal(&h, NULL);
    sc_gfx_new_dummy();

    sc_set_out_handle(h);
    // Seems fine so far, maybe some better testing though imo...?
    test_syscall();


    while (1);

    return PROC_ES_SUCCESS;
}
