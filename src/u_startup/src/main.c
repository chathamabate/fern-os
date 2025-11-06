
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_kb.h"
#include "u_startup/test/syscall_fs.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "u_startup/test/syscall.h"
#include "s_data/test/map.h"
#include "u_concur/test/mutex.h"
#include "u_startup/test/syscall_fut.h"

#include <stdarg.h>

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_set_out_handle(cd);

    test_syscall();

    return PROC_ES_SUCCESS;
}
