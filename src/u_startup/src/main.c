
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_vga_cd.h"

#include <stdarg.h>

#include "u_startup/test/syscall.h"
#include "u_startup/test/syscall_fs.h"
#include "s_mem/simple_heap.h"

proc_exit_status_t user_main(void) {
    fernos_error_t err;
    
    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    sc_set_out_handle(cd);

    sc_out_write_s("Hello, World!\n");

    return PROC_ES_SUCCESS;
}
