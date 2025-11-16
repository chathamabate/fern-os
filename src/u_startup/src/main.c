
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

#include "s_util/constraints.h"
#include <stdarg.h>
#include "u_startup/test/syscall_exec.h"
#include "u_startup/test/syscall_default_io.h"
#include "u_startup/test/syscall_cd.h"

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    sc_set_out_handle(cd);

    // Setup heap. Maybe the heap should also be thread safe?
    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    test_syscall_default_io(cd);

    // Try using relative pointers while in the ROOT?
    // Rarely works?? I wish the fat32 tools didn't suck so bad!
    // Maybe we could change some of the behavior??
    //err = sc_fs_touch("/test_apps/a");
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    return PROC_ES_SUCCESS;
}
