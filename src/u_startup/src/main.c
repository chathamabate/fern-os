
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
#include "s_util/ansi.h"
#include "u_concur/test/mutex.h"

proc_exit_status_t user_main(void) {
    fernos_error_t err;

    handle_t h;

    err = sc_vga_cd_open(&h);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    proc_id_t pid;
    err = sc_proc_fork(&pid);

    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    if (pid == FOS_MAX_PROCS) {
        sc_handle_write_s(h, "AYE YO");
        return PROC_ES_SUCCESS;
    }

    while (1);

    return PROC_ES_SUCCESS;
}
