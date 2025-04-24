
#include "s_bridge/ctx.h"

#include "k_sys/page.h"
#include "k_sys/intr.h"
#include "k_bios_term/term.h"

intr_action_t lock_up_action = NULL;

void set_lock_up_action(intr_action_t la) {
    lock_up_action = la;
}

intr_action_t timer_action = NULL;

void set_timer_action(intr_action_t ta) {
    timer_action = ta;
}

void _lock_up_handler(void) {
    disable_intrs(); 

    out_bios_vga(
        vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK),
        "Interrupt occured with no handler!"
    );

    lock_up();
}
