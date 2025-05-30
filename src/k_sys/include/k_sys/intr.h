
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "k_sys/debug.h"
#include "k_sys/io.h"

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

/**
 * This function just calls the halt machine instruction.
 */
void halt_cpu(void);

static inline void halt_loop(void) {
    while (1) {
        halt_cpu();
    }
}

// PIC Stuff...

void pic_remap(int offset1, int offset2);

uint16_t pic_get_irr(void);
uint16_t pic_get_isr(void);

void pic_mask_all(void);
void pic_unmask_all(void);

void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

// These two ONLY send eoi to either the master or the slave!
void pic_send_master_eoi(void);
void pic_send_slave_eoi(void);

// This will send an eoi to master AND to slave if necessary
void pic_send_eoi(uint8_t irq);

/**
 * These can be nested in interrupt handlers.
 */
void _nop_master_irq7_handler(void);
void _nop_slave_irq15_handler(void);
