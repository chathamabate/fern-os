

#include "k_sys/debug.h"
#include "s_bridge/ctx.h"
#include "k_startup/vga_cd.h"
#include "s_util/err.h"
#include "s_util/misc.h"
#include "s_mem/allocator.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "k_startup/kernel.h"
#include <stdint.h>
#include "k_startup/state.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_sys/intr.h"
#include "k_startup/plugin.h"
#include "k_startup/handle.h"
#include "k_startup/plugin.h"
#include "s_util/ps2_scancodes.h"

#include "k_sys/kb.h"

#include "k_startup/gfx.h"

/**
 * This function is pretty special. It is for when the kernel has no threads to schedule.
 *
 * So, it goes into a halted state. The tricky part is that the kernel and "halted" state
 * both have root priveleges. Without a privilege switch, the kernel stack is never reset!
 *
 * This call is an assembly function which resets the kernel stack, then calls 
 * `_return_to_halt_ctx` below.
 */
void return_to_halt_context(void);

/**
 * This function remains in kernel mode, but enables interrupts and resets the kernel stack!
 * Should be used when there are no threads to schedule!
 */
void _return_to_halt_context(void) {
    user_ctx_t halt_ctx = (user_ctx_t) {
        .ds = KERNEL_DATA_SELECTOR,
        .cr3 = get_kernel_pd(),

        .eip = (uint32_t)halt_loop,
        .cs = KERNEL_CODE_SELECTOR,
        .eflags = read_eflags() | (1 << 9),

        // Without a privilege change, the esp and ss fields aren't used :,(

        // This function will be called in assembly which will first reset the 
        // kernel stack.
    };

    return_to_ctx(&halt_ctx);
}

/**
 * This enters the context of the current thread.
 *
 * If there is no current thread, enter the halt context.
 */
static void return_to_curr_thread(void) {
    thread_t *thr = (thread_t *)(kernel->schedule.head);
    if (thr) {
        return_to_ctx(&(thr->ctx));
    }

    // If we make it here, we have no thread to schedule :,(

    return_to_halt_context();
}

void fos_lock_up_action(user_ctx_t *ctx) {
    (void)ctx;
    gfx_out_fatal("Lock Up Triggered");
}

void fos_gpf_action(user_ctx_t *ctx) {
    if (ctx->cr3 == get_kernel_pd()) { // Are we currently in the kernel?
        gfx_out_fatal("Kernel Locking up in GPF Action");
    }

    ks_exit_proc(kernel, PROC_ES_GPF);
    return_to_curr_thread();
}

void fos_pf_action(user_ctx_t *ctx) {
    // NOTE: I believe right now if you blow the kernel stack, the system just straight up
    // crashes. So don't do that. Maybe in the future I could set up some cool double fault 
    // handler with some special stack.

    if (ctx->cr3 == get_kernel_pd()) {
        gfx_out_fatal("Kernel Locking up in PF Action");
    }

    ks_save_ctx(kernel, ctx);

    uint32_t cr2 = read_cr2();
    void *new_base = (void *)ALIGN(cr2, M_4K);

    fernos_error_t err = ks_expand_stack(kernel, new_base);
    if (err != FOS_E_SUCCESS) {
        ks_exit_proc(kernel, PROC_ES_PF);
    }

    return_to_curr_thread();
}

void fos_timer_action(user_ctx_t *ctx) {
    ks_save_ctx(kernel, ctx);
    
    fernos_error_t err = ks_tick(kernel);
    if (err != FOS_E_SUCCESS) {
        term_put_fmt_s("[Timer Error 0x%X]", err);
        ks_shutdown(kernel);
    }

    return_to_curr_thread();
}

void fos_syscall_action(user_ctx_t *ctx, uint32_t id, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    ks_save_ctx(kernel, ctx);
    fernos_error_t err = FOS_E_SUCCESS;

    thread_t *thr = (thread_t *)(kernel->schedule.head);

    switch (id) {
    case SCID_PROC_FORK:
        err = ks_fork_proc(kernel, (proc_id_t *)arg0);
        break;

    case SCID_PROC_EXIT:
        err = ks_exit_proc(kernel, (proc_exit_status_t)arg0);
        break;

    case SCID_PROC_REAP:
        err = ks_reap_proc(kernel, (proc_id_t)arg0, (proc_id_t *)arg1, (proc_exit_status_t *)arg2);
        break;

    case SCID_PROC_EXEC:
        err = ks_exec(kernel, (user_app_t *)arg0, (const void *)arg1, (size_t)arg2);
        break;

    case SCID_SIGNAL:
        err = ks_signal(kernel, (proc_id_t)arg0, (sig_id_t)arg1);
        break;

    case SCID_SIGNAL_ALLOW:
        err = ks_allow_signal(kernel, (sig_vector_t)arg0);
        break;

    case SCID_SIGNAL_WAIT:
        err = ks_wait_signal(kernel, (sig_vector_t)arg0, (sig_id_t *)arg1);
        break;

    case SCID_SIGNAL_CLEAR:
        err = ks_signal_clear(kernel, (sig_vector_t)arg0);
        break;

    case SCID_MEM_REQUEST:
        err = ks_request_mem(kernel, (void *)arg0, (const void *)arg1, (const void **)arg2);
        break;

    case SCID_MEM_RETURN:
        err = ks_return_mem(kernel, (void *)arg0, (const void *)arg1);
        break;

    case SCID_THREAD_EXIT:
        err = ks_exit_thread(kernel, (void *)arg0);
        break;

    case SCID_THREAD_SLEEP:
        err = ks_sleep_thread(kernel, arg0);
        break;

    case SCID_THREAD_SPAWN:
        err = ks_spawn_local_thread(kernel, (thread_id_t *)arg0, 
                (void *(*)(void *))arg1, (void *)arg2);
        break;

    case SCID_THREAD_JOIN:
        err = ks_join_local_thread(kernel, arg0, (thread_join_ret_t *)arg1);
        break;

    case SCID_SET_IN_HANDLE:
        err = ks_set_in_handle(kernel, (handle_t)arg0);
        break;

    case SCID_GET_IN_HANDLE: // Probs easiest to just do inline here.
        thr->ctx.eax = idtb_get(thr->proc->handle_table, thr->proc->in_handle) 
            ? thr->proc->in_handle : FOS_MAX_HANDLES_PER_PROC;
        err = FOS_E_SUCCESS;
        break;

    case SCID_IN_READ:
        err = ks_in_read(kernel, (void *)arg0, (size_t)arg1, (size_t *)arg2);
        break;

    case SCID_IN_WAIT:
        err = ks_in_wait(kernel);
        break;

    case SCID_SET_OUT_HANDLE:
        err = ks_set_out_handle(kernel, (handle_t)arg0);
        break;

    case SCID_GET_OUT_HANDLE: // Probs easiest to just do inline here.
        thr->ctx.eax = idtb_get(thr->proc->handle_table, thr->proc->out_handle) 
            ? thr->proc->out_handle : FOS_MAX_HANDLES_PER_PROC;
        err = FOS_E_SUCCESS;
        break;

    case SCID_OUT_WRITE:
        err = ks_out_write(kernel, (const void *)arg0, (size_t)arg1, (size_t *)arg2);
        break;

    case SCID_OUT_WAIT:
        err = ks_out_wait(kernel);
        break;

    default:
        // Using a more nested structure here...
        if (scid_is_handle_cmd(id)) {
            handle_t h;
            handle_cmd_id_t h_cmd;

            handle_scid_extract(id, &h, &h_cmd);

            id_table_t *handle_table = thr->proc->handle_table;
            handle_state_t *hs = idtb_get(handle_table, h);

            if (!hs) {
                thr->ctx.eax = FOS_E_INVALID_INDEX;
                err = FOS_E_SUCCESS;
            } else { // handle found!
                switch (h_cmd) {
                case HCID_CLOSE:
                    err = hs_close(hs);
                    break;

                case HCID_WRITE:
                    err = hs_write(hs, (const void *)arg0, (size_t)arg1, (size_t *)arg2);
                    break;

                case HCID_WAIT_WRITE_READY:
                    err = hs_wait_write_ready(hs);
                    break;

                case HCID_READ:
                    err = hs_read(hs, (void *)arg0, (size_t)arg1, (size_t *)arg2);
                    break;

                case HCID_WAIT_READ_READY:
                    err = hs_wait_read_ready(hs);
                    break;

                case HCID_IS_CD:
                    thr->ctx.eax = (uint32_t)(hs->is_cd) ? FOS_E_SUCCESS : FOS_E_UNKNWON_ERROR;
                    err = FOS_E_SUCCESS;
                    break;

                default: // Otherwise, default to custom command!
                    err = hs_cmd(hs, h_cmd, arg0, arg1, arg2, arg3);
                    break;
                }
            }
        } else if (scid_is_plugin_cmd(id)) {
            plugin_id_t plg_id;
            plugin_cmd_id_t plg_cmd_id;

            plugin_scid_extract(id, &plg_id, &plg_cmd_id);

            plugin_t *plg = kernel->plugins[plg_id];

            if (!plg) {
                thr->ctx.eax = FOS_E_INVALID_INDEX;
                err = FOS_E_SUCCESS;
            } else {
                err = plg_cmd(plg, plg_cmd_id, arg0, arg1, arg2, arg3);
            }
        } else {
            thr->ctx.eax = FOS_E_BAD_ARGS;
            err = FOS_E_SUCCESS;
        }

        break;
    }

    if (err != FOS_E_SUCCESS) {
        term_put_fmt_s("[Syscall Error (Syscall: 0x%X, Error: 0x%X)]", id, err);
        ks_shutdown(kernel);
    }

    return_to_curr_thread();
}

void fos_irq1_action(user_ctx_t *ctx) {
    ks_save_ctx(kernel, ctx);

    fernos_error_t err;

    // Kinda interesting point here, but during keyboard init
    // the interrupt will be triggered due to the intial PS/2 commands
    // we send through the I8042.
    //
    // We'll just check again always to see if there actually is data to read.

    uint8_t sr = inb(I8042_R_STATUS_REG_PORT);

    if (sr & I8042_STATUS_REG_OBF) {
        scs1_code_t sc = 0;

        uint8_t recv_byte = inb(I8042_R_OUTPUT_PORT);

        sc |= recv_byte;

        if (recv_byte == SCS1_EXTEND_PREFIX) {
            i8042_wait_for_full_output_buffer();
            recv_byte = inb(I8042_R_OUTPUT_PORT);

            sc <<= 8;
            sc |= recv_byte;
        }

        plugin_t *plg_kb = kernel->plugins[PLG_KEYBOARD_ID];

        if (plg_kb) {
            err = plg_kernel_cmd(plg_kb, PLG_KB_KCID_KEY_EVENT, sc, 0, 0, 0);
            if (err != FOS_E_SUCCESS) {
                term_put_fmt_s("[KeyEvent Error 0x%X]\n", err);
                ks_shutdown(kernel);
            }
        }
    }

    return_to_curr_thread();
}

