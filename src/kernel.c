 
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
    disable_bios_cursor();
    term_set_cursor(true);
    term_clear();

    // This is BIG!!!
    // WOOOOOO
}
