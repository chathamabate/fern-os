

#include "k_sys/debug.h"
#include "s_bridge/ctx.h"
#include "k_bios_term/term.h"
#include "s_util/err.h"
#include "s_util/misc.h"
#include "s_util/ansii.h"
#include "s_util/str.h"
#include "s_mem/allocator.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "k_startup/process.h"
#include "k_startup/kernel.h"
#include <stdint.h>
#include "k_startup/state.h"
#include "k_sys/debug.h"
#include "s_data/wait_queue.h"
#include "k_startup/thread.h"
#include "k_startup/gdt.h"
#include "k_sys/intr.h"
#include "s_util/constraints.h"


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
    if (kernel->curr_thread) {
        return_to_ctx(&(kernel->curr_thread->ctx));
    }

    // If we make it here, we have no thread to schedule :,(

    return_to_halt_context();
}

void fos_lock_up_action(user_ctx_t *ctx) {
    (void)ctx;
    out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), "Lock Up Triggered");
    lock_up();
}

void fos_gpf_action(user_ctx_t *ctx) {
    if (ctx->cr3 == get_kernel_pd()) { // Are we currently in the kernel?
        out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), 
                "Kernel Locking up in GPF Action");
        lock_up();
    }

    ks_exit_proc(kernel, PROC_ES_GPF);
    return_to_curr_thread();
}

void fos_pf_action(user_ctx_t *ctx) {
    if (ctx->cr3 == get_kernel_pd()) {
        out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), 
                "Kernel Locking up in PF Action");
        lock_up();
    }

    ks_save_ctx(kernel, ctx);

    uint32_t cr2 = read_cr2();
    void *new_base = (void *)ALIGN(cr2, M_4K);

    fernos_error_t err = ks_expand_stack(kernel, new_base);
    if (err != FOS_SUCCESS) {
        ks_exit_proc(kernel, PROC_ES_PF);
    }

    return_to_curr_thread();
}

void fos_timer_action(user_ctx_t *ctx) {
    ks_save_ctx(kernel, ctx);
    
    fernos_error_t err = ks_tick(kernel);
    if (err != FOS_SUCCESS) {
        out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), 
                "Fatal timer error");
        lock_up();
    }

    return_to_curr_thread();
}

void fos_syscall_action(user_ctx_t *ctx, uint32_t id, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    ks_save_ctx(kernel, ctx);
    fernos_error_t err = FOS_SUCCESS;

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

    case SCID_SIGNAL:
        err = ks_signal(kernel, (proc_id_t)arg0, (sig_id_t)arg1);
        break;

    case SCID_SIGNAL_ALLOW:
        err = ks_allow_signal(kernel, (sig_vector_t)arg0);
        break;

    case SCID_SIGNAL_WAIT:
        err = ks_wait_signal(kernel, (sig_vector_t)arg0, (sig_id_t *)arg1);
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

    case SCID_FUTEX_REGISTER:
        err = ks_register_futex(kernel, (futex_t *)arg0);
        break;

    case SCID_FUTEX_DEREGISTER:
        err = ks_deregister_futex(kernel, (futex_t *)arg0);
        break;

    case SCID_FUTEX_WAIT:
        err = ks_wait_futex(kernel, (futex_t *)arg0, arg1);
        break;

    case SCID_FUTEX_WAKE:
        err = ks_wake_futex(kernel, (futex_t *)arg0, (bool)arg1);
        break;

    case SCID_TERM_PUT_S:
        if (!arg0) {
            kernel->curr_thread->ctx.eax = FOS_BAD_ARGS;
            err = FOS_SUCCESS;
            break;
        }

        char *buf = da_malloc(arg1);
        mem_cpy_from_user(buf, ctx->cr3, (const void *)arg0, arg1, NULL);
        term_put_s(buf);
        da_free(buf);

        break;

    default:
        err = FOS_BAD_ARGS;
        break;
    }

    if (err != FOS_SUCCESS) {
        term_put_fmt_s("[Syscall Error (Syscall: 0x%X, Error: 0x%X)]", id, err);
        lock_up();
    }

    return_to_curr_thread();
}

