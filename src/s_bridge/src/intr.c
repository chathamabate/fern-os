
#include "s_bridge/intr.h"
#include "k_bios_term/term.h"
#include "k_sys/page.h"
#include "k_sys/intr.h"

const phys_addr_t intr_pd;
const uint32_t * const intr_esp;

void set_intr_ctx(phys_addr_t pd, const uint32_t *esp) {
    *(phys_addr_t *)&intr_pd = pd;
    *(const uint32_t **)&intr_esp = esp;
}

uint32_t enter_intr_ctx(phys_addr_t curr_pd) {
    if (curr_pd != intr_pd) {
        
    }
    return 0;
}

void _lock_up_handler(void) {
    disable_intrs(); 

    out_bios_vga(
        vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK),
        "Interrupt occured with no handler!"
    );

    lock_up();
}

void nop_master_irq_handler(void);
void nop_master_irq7_handler(void); // For spurious.

void nop_slave_irq_handler(void);
void nop_slave_irq15_handler(void); // For spurious.

