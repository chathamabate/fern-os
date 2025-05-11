

#include "k_sys/debug.h"
#include "s_bridge/ctx.h"
#include "k_bios_term/term.h"
#include "s_util/err.h"
#include "s_util/misc.h"
#include "s_util/ansii.h"
#include "s_util/str.h"
#include "s_bridge/syscall.h"
#include "s_mem/allocator.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "k_startup/process.h"
#include "k_startup/kernel.h"
#include <stdint.h>
#include "k_startup/state.h"
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

// A lot more to build up here, although, tbh, we aren't that far from
// being done...

void fos_gpf_action(user_ctx_t *ctx) {
    (void)ctx;
    out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), "GPF/Interrupt with no handler");
    lock_up();
}

void fos_timer_action(user_ctx_t *ctx) {
    ks_save_ctx(kernel, ctx);
    
    fernos_error_t err = ks_tick(kernel);
    if (err != FOS_SUCCESS) {
        term_put_s("AHHHH\n");
        lock_up();
    }

    return_to_curr_thread();
}

void fos_syscall_action(user_ctx_t *ctx, uint32_t id, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    ks_save_ctx(kernel, ctx);
    fernos_error_t err;

    switch (id) {
    case SCID_THREAD_EXIT:
        err = ks_exit_thread(kernel, (void *)arg0);
        if (err != FOS_SUCCESS) {
            term_put_fmt_s("Exit Failed %u\n", err);
            lock_up();
        }

        break;

    case SCID_THREAD_SLEEP:
        err = ks_sleep_thread(kernel, arg0);

        if (err != FOS_SUCCESS) {
            term_put_s("BAD SLEEP CALL\n");
            lock_up();
        }
        
        break;

    case SCID_THREAD_SPAWN:
        err = ks_spawn_local_thread(kernel, (thread_id_t *)arg0, 
                (void *(*)(void *))arg1, (void *)arg2);

        if (err != FOS_SUCCESS) {
            term_put_s("Bad Thread Creation\n");
            lock_up();
        }

        break;

    case SCID_THREAD_JOIN:
        err = ks_join_local_thread(kernel, arg0, (thread_join_ret_t *)arg1);

        if (err != FOS_SUCCESS) {
            term_put_s("error with join\n");
            lock_up();
        }

        break;

    case SCID_TERM_PUT_S:
        if (!arg0) {
            term_put_s("NULL str\n");
            lock_up();
        }

        char *buf = da_malloc(arg1);
        mem_cpy_from_user(buf, ctx->cr3, (const void *)arg0, arg1, NULL);
        term_put_s(buf);
        da_free(buf);

        break;

    default:
        term_put_s("Unknown syscall!\n");
        lock_up();

    }

    return_to_curr_thread();
}

