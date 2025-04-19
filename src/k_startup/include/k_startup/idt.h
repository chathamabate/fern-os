
#pragma once

#include "k_sys/dt.h"
#include "k_sys/idt.h"

#include <stdint.h>

#define NUM_IDT_ENTRIES 0x100

extern uint8_t _idt_start[];
extern uint8_t _idt_end[];

/**
 * Initialize and load the interrupt descriptor table.
 */
fernos_error_t init_idt(void);
