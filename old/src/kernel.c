 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#include "msys/intr.h"
#include "util/str.h"
#include "msys/page.h"

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "Please compile with a ix86-elf compiler"
#endif

#include "msys/dt.h"
#include "msys/debug.h"
#include "terminal/out.h"

static void print_gdt(void) {
    char buf[100];
    gdtr_val_t v;

    read_gdtr(&v);

    size_t len = (v.size_m_1 + 1) / 8;
    seg_descriptor_t *tbl = (v.offset);

    str_fmt(buf, "Num Entries: %u\n", len);
    term_put_s(buf);

    for (uint32_t i = 0; i < len; i++) {
        seg_descriptor_t sd = tbl[i];
        
        uint32_t lim = sd_get_limit(sd);
        uint32_t base = sd_get_base(sd);
        uint32_t flags = sd_get_flags(sd);
        uint32_t access_byte = sd_get_access_byte(sd);

        str_fmt(buf, "\nEntry %u:\n", i);
        term_put_s(buf);

        str_fmt(buf, "  Range = (%X, %X)\n", base, lim);
        term_put_s(buf);

        str_fmt(buf, "  Flags = %X AB = %X\n", flags, access_byte);
        term_put_s(buf);
    }
}

static void print_idt(void) {
    char buf[100];
    idtr_val_t v;
    read_idtr(&v);

    size_t len = (v.size_m_1 + 1) / 8;
    gate_descriptor_t *tbl = (v.offset);

    str_fmt(buf, "Num Entries: %u @ %X\n", len, tbl);
    term_put_s(buf);

    for (size_t i = 0; i < 16; i++) {
        gate_descriptor_t gd = tbl[i];
        uint32_t selector = gd_get_selector(gd);
        uint32_t offset = gd_get_offset(gd);
        uint32_t attrs = gd_get_attrs(gd);

        str_fmt(buf, "\nEntry %u:\n", i);
        term_put_s(buf);

        str_fmt(buf, "  Sel: %X, Off: %X, Attrs: %X\n",
                selector, offset, attrs);
        term_put_s(buf);
    }
}


void kernel_main(void) {
    term_init();


    term_put_s("HEllo\n");

    // AYE YO THIS WORKS!!!!
    *(uint8_t *)(0x1000000) = 10;

    return;
}


