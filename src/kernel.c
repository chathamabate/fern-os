 
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

    if (test_str()) {
        term_puts("SUCCESS");
    } else {
        term_puts("FAILURE");
    }
}
