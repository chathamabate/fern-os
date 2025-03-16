
#include "k_startup/init.h"
#include "k_sys/dt.h"
#include "k_sys/page.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"
#include "k_sys/gdt.h"
#include "term/term.h"
#include "term/term_sys_helpers.h"
#include "os_defs.h"

#define NUM_GDT_ENTRIES 0x10
static seg_desc_t gdt[NUM_GDT_ENTRIES] __attribute__((aligned(0x8)));

static void init_gdt(void) {
    for (size_t i = 0; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = not_present_seg_desc();
    }

    exec_seg_desc_t *k_startup_code = &(gdt[1]);  // 0x08
    *k_startup_code = exec_seg_desc();

    sd_set_base(k_startup_code, 0x0);
    sd_set_limit(k_startup_code, 0x80000);
    sd_set_gran(k_startup_code, 0x1);
    sd_set_privilege(k_startup_code, 0x0);

    esd_set_def(k_startup_code, 0x1);
    esd_set_readable(k_startup_code, 0x1);
    esd_set_conforming(k_startup_code, 0x0);

    data_seg_desc_t *k_startup_data = &(gdt[2]);  // 0x10
    *k_startup_data = data_seg_desc();

    sd_set_base(k_startup_data, 0x0);
    sd_set_limit(k_startup_data, 0x80000);
    sd_set_gran(k_startup_data, 0x1);
    sd_set_privilege(k_startup_data, 0x0);

    dsd_set_ex_down(k_startup_data, 0x0);
    dsd_set_writable(k_startup_data, 0x1);
    dsd_set_big(k_startup_data, 0x1);

    exec_seg_desc_t *user_code = &(gdt[3]);  // 0x18
    *user_code = exec_seg_desc();

    sd_set_base(user_code, 0x0);
    sd_set_limit(user_code, 0x80000);
    sd_set_gran(user_code, 0x1);
    sd_set_privilege(user_code, 0x3);

    esd_set_def(user_code, 0x1);
    esd_set_readable(user_code, 0x1);
    esd_set_conforming(user_code, 0x0);

    data_seg_desc_t *user_data = &(gdt[4]);  // 0x20
    *user_data = data_seg_desc();

    sd_set_base(user_data, 0x0);
    sd_set_limit(user_data, 0x80000);
    sd_set_gran(user_data, 0x1);
    sd_set_privilege(user_data, 0x3);

    dsd_set_ex_down(user_data, 0x0);
    dsd_set_writable(user_data, 0x1);
    dsd_set_big(user_data, 0x1);

    // Useful experiments
    /*
    data_seg_desc_t *high_data = &(gdt[5]);  // 0x28
    *high_data = data_seg_desc();

    sd_set_base(high_data, 0x80000000);
    sd_set_limit(high_data, 0x80000);
    sd_set_gran(high_data, 0x1);
    sd_set_privilege(high_data, 0x03);

    dsd_set_ex_down(high_data, 0x0);
    dsd_set_writable(high_data, 0x1);
    dsd_set_big(high_data, 0x1);
    */

    dtr_val_t dtv = dtr_val();

    dtv_set_base(&dtv, gdt);
    dtv_set_num_entries(&dtv, NUM_GDT_ENTRIES);

    load_gdtr(dtv);
}

static void dflt_intr_handler(void) {
    disable_intrs();

    const char *msg = "Interrupt occured with no handler!";
    const char *iter = msg;
    volatile uint16_t *t = TERMINAL_BUFFER;

    char c;
    while ((c = *(iter++)) != '\0') {
        *(t++) = vga_entry(c, vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    }

    lock_up();
}

#define NUM_IDT_ENTRIES 0x100
static seg_desc_t idt[NUM_IDT_ENTRIES] __attribute__((aligned(0x8)));

static void init_idt(void) {
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
}


void init_paging(void) {

}


void init_all(void) {
    // NOTE: This should be basically the first function called
    // after loading from grub. Grub enters the k_startup with interrupts
    // disabled.
    //
    // So this is slighlty redundant, but whatever.
    disable_intrs();

    init_gdt();

    init_idt();

    term_init();

    enable_intrs();
}

