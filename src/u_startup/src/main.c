
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall_kb.h"

#include "s_mem/simple_heap.h"

#include "s_util/constraints.h"
#include "s_util/ps2_scancodes.h"
#include <stdarg.h>

#include "u_startup/test/syscall_fs.h"
#include "u_startup/test/syscall_default_io.h"

proc_exit_status_t user_main(void) {
    fernos_error_t err;
    
    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    sc_set_out_handle(cd);
    // We need a simple heap to call exec!
    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }
    test_syscall_fs();
    return PROC_ES_SUCCESS;

    /*
    sc_signal_allow(1 << FSIG_CHLD);

    proc_id_t shell_cpid;
    err = sc_proc_fork(&shell_cpid);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    if (shell_cpid == FOS_MAX_PROCS) { // Child process.
        handle_t kb;
        err = sc_kb_open(&kb);
        if (err != FOS_E_SUCCESS) {
            return PROC_ES_FAILURE;
        }
        sc_set_in_handle(kb);

        handle_t cd;
        err = sc_vga_cd_open(&cd);
        if (err != FOS_E_SUCCESS) {
            return PROC_ES_FAILURE;
        }
        sc_set_out_handle(cd);

        // We need a simple heap to call exec!
        err = setup_default_simple_heap(USER_MMP);
        if (err != FOS_E_SUCCESS) {
            return PROC_ES_FAILURE;
        }

        err = sc_fs_exec_da_elf32_va("/Bin/Shell");

        // Should never make it here!
        return PROC_ES_FAILURE;
    }

    // The root process will just reap now after spawning a shell!

    while (true) {
        err = sc_signal_wait(1 << FSIG_CHLD, NULL);
        if (err == FOS_E_SUCCESS) {
            proc_id_t rpid;
            while ((err = sc_proc_reap(FOS_MAX_PROCS, &rpid, NULL)) == FOS_E_SUCCESS) {
                if (rpid == shell_cpid) {
                    return PROC_ES_SUCCESS; // Exit system when we've reaped the shell!
                }
            }

            if (err != FOS_E_EMPTY) { // should never happen.
                return PROC_ES_FAILURE;
            }
        }
    }
    */
}
