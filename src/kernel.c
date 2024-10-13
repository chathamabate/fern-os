 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#include "util/str.h"
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "Please compile with a ix86-elf compiler"
#endif

#include "msys/gdt.h"
#include "terminal/out.h"

extern void write(void *addr);

void kernel_main(void) {
    term_init();
    term_clear();

    gdtr_val_t v;

    read_gdtr(&v);

    char buf[20];
    str_fmt(buf, "Offset = 0x%X\n", (v.offset));
    term_puts(buf);

    str_fmt(buf, "Size   = 0x%X\n", (uint32_t)(v.size));
    term_puts(buf);
}
