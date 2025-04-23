
#include "s_bridge/ctx.h"

#include "k_sys/page.h"
#include "k_sys/intr.h"
#include "k_bios_term/term.h"

phys_addr_t intr_ctx_pd = NULL_PHYS_ADDR;

void set_intr_ctx_pd(phys_addr_t pd) {
    intr_ctx_pd = pd;
}

void _lock_up_handler(void) {
    disable_intrs(); 

    out_bios_vga(
        vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK),
        "Interrupt occured with no handler!"
    );

    lock_up();
}
