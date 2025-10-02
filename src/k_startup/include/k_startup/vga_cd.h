
#pragma once

#include "s_util/err.h"
#include "s_util/char_display.h"

/**
 * Write a string directly to the BIOS VGA Terminal.
 *
 * This can be used at any time!
 *
 * No setup is required.
 */
void out_bios_vga(char_display_style_t style, const char *str);

extern char_display_t * const VGA_CD;

/**
 * VGA_CD is unusable until this function is called.
 */
fernos_error_t init_vga_char_display(void);

/*
 * Prior to this more generic Character display design, there were a 
 * set of functions which always printed to the VGA display directly.
 *
 * The below functions are to maintain backward compatibility where
 * said functions were used.
 */

static inline void term_put_c(char c) {
    cd_put_c(VGA_CD, c);
}

static inline void term_put_s(const char *s) {
    cd_put_s(VGA_CD, s);
}

#define TERM_FMT_BUF_SIZE 1024
void term_put_fmt_s(const char *fmt, ...);

#include "k_sys/dt.h"
#include "k_sys/idt.h"
#include "k_sys/gdt.h"
#include "k_sys/page.h"

/**
 * Print out the values in the stack before this function is called.
 * slots is the number of 32-bit values to trace up the stack.
 */
void term_put_trace(uint32_t slots, uint32_t *esp);

void term_put_seg_selector(seg_selector_t ssr);
void term_put_seg_desc(seg_desc_t sd);
void term_put_gate_desc(gate_desc_t gd);
void term_put_dtr(dtr_val_t dtv);
void term_put_pte(pt_entry_t pte);
