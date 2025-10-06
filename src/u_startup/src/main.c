
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_cd.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/test/syscall.h"

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    // Ok, now what??
    // I mean, is user malloc so bad??

    sc_set_out_handle(cd);

    test_syscall();

    return PROC_ES_SUCCESS;
}
