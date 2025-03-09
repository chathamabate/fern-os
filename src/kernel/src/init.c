
#include "kernel/init.h"
#include "msys/dt.h"
#include "msys/intr.h"
#include "msys/gdt.h"
#include "term/term.h"
#include "term/term_sys_helpers.h"

#define NUM_GDT_ENTRIES 0x10
static seg_desc_t gdt[NUM_GDT_ENTRIES] __attribute__((aligned(0x8)));

static void init_gdt(void) {
    for (size_t i = 0; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = not_present_seg_desc();
    }

    exec_seg_desc_t *kernel_code = &(gdt[1]);  // 0x08
    *kernel_code = exec_seg_desc();

    sd_set_base(kernel_code, 0x0);
    sd_set_limit(kernel_code, 0xFFFFF);
    sd_set_gran(kernel_code, 0x1);
    sd_set_privilege(kernel_code, 0x0);

    esd_set_def(kernel_code, 0x1);
    esd_set_readable(kernel_code, 0x1);
    esd_set_conforming(kernel_code, 0x0);

    data_seg_desc_t *kernel_data = &(gdt[2]);  // 0x10
    *kernel_data = data_seg_desc();

    sd_set_base(kernel_data, 0x0);
    sd_set_limit(kernel_data, 0xFFFFF);
    sd_set_gran(kernel_data, 0x1);
    sd_set_privilege(kernel_data, 0x0);

    dsd_set_ex_down(kernel_data, 0x0);
    dsd_set_writable(kernel_data, 0x1);
    dsd_set_big(kernel_data, 0x1);

    exec_seg_desc_t *user_code = &(gdt[3]);  // 0x18
    *user_code = exec_seg_desc();

    sd_set_base(user_code, 0x0);
    sd_set_limit(user_code, 0xFFFFF);
    sd_set_gran(user_code, 0x1);
    sd_set_privilege(user_code, 0x3);

    esd_set_def(user_code, 0x1);
    esd_set_readable(user_code, 0x1);
    esd_set_conforming(user_code, 0x0);

    data_seg_desc_t *user_data = &(gdt[4]);  // 0x20
    *user_data = data_seg_desc();

    sd_set_base(user_data, 0x0);
    sd_set_limit(user_data, 0xFFFFF);
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
    sd_set_limit(high_data, 0xFFFFF);
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

    for (uint32_t i = 0; i < NUM_IDT_ENTRIES; i++) {
        idt[i] = gd;    
    }

    // Handler which does nothing. (Just calls iret)
    intr_gate_desc_t ngd = intr_gate_desc();
    gd_set_selector(&ngd, 0x8);
    gd_set_privilege(&ngd, 0);
    igd_set_base(&ngd, nop_handler);

    // Remeber, double fault intially causes problems!
    idt[0x8] = ngd;

    dtr_val_t dtv = dtr_val();
    dtv_set_base(&dtv, idt);
    dtv_set_num_entries(&dtv, NUM_IDT_ENTRIES);

    load_idtr(dtv);
}

void init_all(void) {
    disable_intrs();

    init_gdt();
    init_idt();

    term_init();

    enable_intrs();
}

