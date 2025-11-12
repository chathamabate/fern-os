
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

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_set_out_handle(cd);

    // Setup heap.
    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    user_app_t *ua;
    err = sc_fs_parse_elf32(get_default_allocator(), 
            "/core_apps/dummy_app", &ua);  
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    const char *args[] = {
        "hello",
        "world"
    };
    const size_t num_args = sizeof(args) / sizeof(args[0]);

    const void *args_block;
    size_t args_block_len;
    
    err = new_da_args_block(args, num_args, &args_block, &args_block_len);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    args_block_make_absolute((void *)args_block, FOS_APP_ARGS_AREA_START);

    err = sc_proc_exec(ua, args_block, args_block_len);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    return PROC_ES_SUCCESS;
}
