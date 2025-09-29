
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
#include "s_util/ansi.h"
#include "u_concur/test/mutex.h"

proc_exit_status_t user_main(void) {
    fernos_error_t err;

    handle_t h;

    err = sc_vga_cd_open(&h);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    size_t rows;
    size_t cols;

    sc_cd_get_dimmensions(h, &rows, &cols);

    for (size_t i = 0; i < rows - 1; i++) {
        sc_handle_write_s(h, "\na");
    }

    while (1);

    return PROC_ES_SUCCESS;
}
