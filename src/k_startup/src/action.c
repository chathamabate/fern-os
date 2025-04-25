

#include "k_startup/action.h"
#include "s_bridge/ctx.h"
#include "k_bios_term/term.h"
#include "s_util/misc.h"
#include "s_util/ansii.h"

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
    term_put_fmt_s("Syscall ID: %u, Arg %u\n", id, arg);
    return_to_ctx_with_value(ctx, 10);
}

