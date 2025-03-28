
#include "k_startup/page.h"
#include "k_sys/idt.h"
#include "s_util/err.h"
#include "s_util/test/str.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
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

    if (init_term() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Terminal");
        lock_up();
    }

    if (init_paging() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Paging");
        lock_up();
    }

    /*
    fernos_error_t err = init_paging();
    if (err != FOS_SUCCESS) {
        term_put_fmt_s("Couldn't set this up %X\n", err);
        //out_bios_vga(init_err_style, "Failed to set up Paging");
        lock_up();

    }
    */
}

void _bad_handler(void) {
    term_put_s("Hello From Bad Handler\n");
}

void timer_handler(void);

void bad_handler(void);

int rec_thing(void) {
    term_put_s("Aye yo\n");
    return rec_thing() + 1;
}

int kernel_main(void) {
    term_put_s("HEllo\n");
    dss();

    // Wooo this works!!
   // *(int *)(0xC0000000 - 0x4001) = 10;

    // The handler it self uses the stack!
    //*(int *)(0x1000000) = 1232;

    return 0;
}

