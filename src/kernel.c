 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

#include "terminal/out.h"
#include "msys/io.h"

void kernel_main(void) {
    term_init();
    term_clear();

    term_set_output_style(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    term_puts("Hello");

    term_set_cursor(0, 0);
    term_set_output_style(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
    term_puts("He");

    // This is BIG!!!
    // WOOOOOO
}
