
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "msys/debug.h"
#include "msys/io.h"

void enable_intrs(void);
void disable_intrs(void);

static inline uint32_t intrs_enabled(void) {
    return (read_eflags() & (1 << 9)) != 0;
}

// The idea of an "interrupt section" is to restore the correct value
// of the interrupt enable flag after leaving the section.

static inline uint32_t intr_section_enter(void) {
    uint32_t en = intrs_enabled();
    disable_intrs();
    return en;
}

static inline void intr_section_exit(uint32_t en) {
    if (en) {
        enable_intrs();
    }
}

// NOTE: This handler just calls iret.
// It will NOT work for situations where an error code is pushed onto the stack.
void nop_handler(void);

// PIC Stuff...

void pic_send_eoi(uint8_t irq);

void pic_remap(int offset1, int offset2);

uint16_t pic_get_irr(void);
uint16_t pic_get_isr(void);

void pic_mask_all(void);
void pic_unmask_all(void);

void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
