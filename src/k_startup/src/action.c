

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
    // When a timer interrupt is received, we must notify the sleep
    // queue with the current time!

    fernos_error_t err;

    // First things first, if we were executing a real thread, save its state!
    if (kernel->curr_thread) {
        kernel->curr_thread->ctx = *ctx; 
    };

    kernel->curr_tick++;

    twq_notify(kernel->sleep_q, kernel->curr_tick);

    thread_t *woken_thread;
    while ((err = twq_pop(kernel->sleep_q, (void **)&woken_thread)) == FOS_SUCCESS) {
        // Prepare to be scheduled!
        woken_thread->next_thread = NULL;
        woken_thread->prev_thread = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        ks_schedule_thread(kernel, woken_thread);
    }

    term_put_fmt_s("ADDR: %X\n", &woken_thread);

    if (err != FOS_EMPTY) {
        // TODO Do something here...
        term_put_s("Error waking up threads?\n");
        lock_up();
    }

    if (kernel->curr_thread) {
        kernel->curr_thread = kernel->curr_thread->next_thread;
    }

    return_to_curr_thread();
}

void fos_syscall_action(user_ctx_t *ctx, uint32_t id, void *arg) {
    fernos_error_t err;
    buffer_arg_t buf_arg;
    thread_t *thr;

    switch (id) {
    case SCID_THREAD_EXIT:
        term_put_fmt_s("Thread Exited with arg %u\n", arg);
        lock_up();

    case SCID_THREAD_SLEEP:
        thr = kernel->curr_thread; // Gauranteed non-null.
        thr->ctx = *ctx;           // Save context

        ks_deschedule_thread(kernel, thr);
        err = twq_enqueue(kernel->sleep_q, (void *)thr, 
                kernel->curr_tick + (uint32_t)arg);

        if (err != FOS_SUCCESS) {
            term_put_s("BAD SLEEP CALL\n");
            lock_up();
        }

        return_to_curr_thread();

    case SCID_TERM_PUT_S:
        if (!arg) {
            term_put_s("NULL str\n");
            lock_up();
        }
        
        // Pretty cool this works!
        // No error checking though at the moment :,(
        mem_cpy_from_user(&buf_arg, ctx->cr3, arg, sizeof(buffer_arg_t), NULL);
        char *buf = da_malloc(buf_arg.buf_size);
        mem_cpy_from_user(buf, ctx->cr3, buf_arg.buf, buf_arg.buf_size, NULL);
        term_put_s(buf);
        da_free(buf);

        return_to_ctx(ctx);

    default:
        term_put_s("Unknown syscall!\n");
        lock_up();
    }
}

