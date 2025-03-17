
#include "s_util/err.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_startup/page.h"
#include "term/term.h"

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

    /*
    if (init_paging() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Paging");
        lock_up();
    }
    */

    if (init_term() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Terminal");
        lock_up();
    }
}

int kernel_main(void) {
    term_put_s("Hello World\n");
    return 0;
}

