
#include "u_startup/main.h"
#include "s_util/misc.h"
#include "s_util/str.h"
#include "u_concur/mutex.h"
#include "s_util/constraints.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/test/syscall.h"
#include "u_startup/test/syscall_fs.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_cd.h"
#include "u_startup/syscall_kb.h"
#include "s_util/ansi.h"
#include "u_concur/test/mutex.h"
#include "s_util/ps2_scancodes.h"
#include "u_startup/test/syscall_cd.h"
#include "u_startup/test/syscall_default_io.h"
#include "u_startup/test/syscall_kb.h"

proc_exit_status_t user_main(void) {
    fernos_error_t err;

    handle_t cd;

    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    sc_set_out_handle(cd);
    
    test_kb_simple_prompt();

    return PROC_ES_SUCCESS;
}
