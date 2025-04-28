

#include "k_sys/debug.h"
#include "s_bridge/ctx.h"
#include "k_bios_term/term.h"
#include "s_util/err.h"
#include "s_util/misc.h"
#include "s_util/ansii.h"
#include "s_bridge/syscall.h"
#include "s_mem/allocator.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include <stdint.h>

void fos_gpf_action(user_ctx_t *ctx) {
    (void)ctx;
    out_bios_vga(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK), "GPF/Interrupt with no handler");
    lock_up();
}

void fos_timer_action(user_ctx_t *ctx) {
    term_put_fmt_s("Timer\n", ctx);
    return_to_ctx(ctx);
}

void fos_syscall_action(user_ctx_t *ctx, uint32_t id, void *arg) {
    fernos_error_t err;
    buffer_arg_t buf_arg;

    switch (id) {
    case SCID_THREAD_EXIT:
        term_put_fmt_s("Thread Exited with arg %u\n", arg);
        lock_up();

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
        break;

    default:
        term_put_s("Unknown syscall!\n");
        lock_up();
    }
}

