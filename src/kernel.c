 
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

    size_t len = (v.size_m_1 + 1) / 8;
    seg_descriptor_t *tbl = (seg_descriptor_t *)(v.offset);

    char buf[100];
    str_fmt(buf, "Num Entries: %u\n", len);
    term_puts(buf);

    // Looks like in this configuration, the entire memory space is read/write/executable!
    // Looks like this is for paging setup at somepoint??
    // Maybe I could change this at some point?
    // Attempt changing this to some different value??

    for (uint32_t i = 0; i < len; i++) {
        seg_descriptor_t sd = tbl[i];
        
        uint32_t lim = sd_get_limit(sd);
        uint32_t base = sd_get_base(sd);
        uint32_t flags = sd_get_flags(sd);
        uint32_t access_byte = sd_get_access_byte(sd);

        str_fmt(buf, "\nEntry %u:\n", i);
        term_puts(buf);

        str_fmt(buf, "  Base  = %X\n", base);
        term_puts(buf);

        str_fmt(buf, "  Limit = %X\n", lim);
        term_puts(buf);

        str_fmt(buf, "  Flags = %X\n", flags);
        term_puts(buf);

        str_fmt(buf, "  AB    = %X\n", access_byte);
        term_puts(buf);
    }

    return;
}
