
#include "k_startup/idt.h"
#include "k_sys/dt.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"
#include "s_util/err.h"
#include "k_sys/debug.h"

seg_desc_t idt[NUM_IDT_ENTRIES] __attribute__ ((aligned(8)));

static void dflt_intr_handler(void) {
    disable_intrs();

    out_bios_vga(
        vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK),
        "Interrupt occured with no handler!"
    );

    lock_up();
}

fernos_error_t init_idt(void) {
    intr_gate_desc_t gd = intr_gate_desc();

    gd_set_selector(&gd, 0x8);
    gd_set_privilege(&gd, 0);
    igd_set_base(&gd, dflt_intr_handler);

    // First, all nops.
    for (uint32_t i = 0; i < NUM_IDT_ENTRIES; i++) {
        idt[i] = gd;    
    }

    // 0 - 6

    for (uint32_t i = 32; i < 39; i++) {
        intr_gate_desc_t master_irq_gd = intr_gate_desc();

        gd_set_selector(&master_irq_gd, 0x8);
        gd_set_privilege(&master_irq_gd, 0);
        igd_set_base(&master_irq_gd, nop_master_irq_handler);

        idt[i] = master_irq_gd;
    }

    // 7

    intr_gate_desc_t master_irq7_gd = intr_gate_desc();

    gd_set_selector(&master_irq7_gd, 0x8);
    gd_set_privilege(&master_irq7_gd, 0);
    igd_set_base(&master_irq7_gd, nop_master_irq7_handler);

    idt[39] = master_irq7_gd;

    // 8 - 14

    for (uint32_t i = 40; i < 47; i++) {
        intr_gate_desc_t slave_irq_gd = intr_gate_desc();

        gd_set_selector(&slave_irq_gd, 0x8);
        gd_set_privilege(&slave_irq_gd, 0);
        igd_set_base(&slave_irq_gd, nop_slave_irq_handler);

        idt[i] = slave_irq_gd;
    }

    // 15 
    
    intr_gate_desc_t slave_irq15_gd = intr_gate_desc();

    gd_set_selector(&slave_irq15_gd, 0x8);
    gd_set_privilege(&slave_irq15_gd, 0);
    igd_set_base(&slave_irq15_gd, nop_slave_irq15_handler);

    idt[47] = slave_irq15_gd;

    dtr_val_t dtv = dtr_val();
    dtv_set_base(&dtv, idt);
    dtv_set_num_entries(&dtv, NUM_IDT_ENTRIES);

    load_idtr(dtv);

    pic_remap(32, 40);
    
    return FOS_SUCCESS;
}
