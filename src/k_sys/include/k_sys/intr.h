
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

// PIT Stuff... These all assume operation in kernel space where interrupts
// are disabled!

/**
 * Initilize the Programmable interval timer.
 *
 * NOTE: You don't NEED to call this function, but you should!
 * It'll determine how often IRQ0 occurs!
 */
void init_pit(uint16_t init_reload_val);

/**
 * Set the reload value on channel 0.
 *
 * IRQ0 will trigger at 1.19 Million / `reload_val` Hz
 *
 * NOTE: This assumes the PIT is setup with lo/hi access mode!
 */
void pit_set_reload_val(uint16_t reload_val);

/**
 * Gets the current count value on channel 0.
 */
uint16_t pit_get_count(void);
