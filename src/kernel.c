 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

#include "terminal/out.h"
#include "util/str.h"
#include "msys/io.h"

void kernel_main(void) {
    term_init();
    term_clear();

    char buf[100];

    // Maybe we can make a nice grid???
    // That could be cool...
    
    // What if we just want to left align a string??
    // This could be helpful!!
    for (size_t i = 0; i < 90; i++) {
        term_set_output_style(
                vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));

        str_of_u_ra(buf, 3, ' ', i);
        term_puts(buf);

        term_set_output_style(
                vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        term_puts(":");

        str_of_u_ra(buf, 10, '_', i * 2);
        term_puts(buf);

        term_cursor_next_line();
    }

}
