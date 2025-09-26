
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

    sc_handle_write_fmt_s(h, "Number: " ANSI_BRIGHT_CYAN_FG ANSI_MAGENTA_BG "%d" ANSI_RESET "\n", 39);

    return PROC_ES_SUCCESS;
}
