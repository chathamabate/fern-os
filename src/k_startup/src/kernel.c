
#include "k_sys/idt.h"
#include "s_util/err.h"
#include "s_util/test/str.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_startup/page.h"
#include "k_bios_term/term.h"

void kernel_init(void) {
    uint8_t init_err_style = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (init_gdt() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up GDT");
        lock_up();
    }

    if (init_idt() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up IDT");
        lock_up();
    }

    if (init_paging() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Paging");
        lock_up();
    }

    if (init_term() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Terminal");
        lock_up();
    }
}

void _bad_handler(void) {
    term_put_s("Hello From Bad Handler\n");
}

void timer_handler(void);

void bad_handler(void);

int kernel_main(void) {
    /*
    intr_gate_desc_t timer = intr_gate_desc();
    gd_set_selector(&timer, 0x8);
    gd_set_privilege(&timer, 0x0);
    igd_set_base(&timer, timer_handler);

    set_gd(32, timer);

    intr_gate_desc_t gd = intr_gate_desc();
    gd_set_selector(&gd, 0x8);
    gd_set_privilege(&gd, 0x0);
    igd_set_base(&gd, bad_handler);

    set_gd(0x1D, gd);
    */
    test_str();


    return 0;
}

