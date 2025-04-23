
#pragma once

#include "k_sys/dt.h"
#include "k_sys/gdt.h"

#define NUM_GDT_ENTRIES 0x10

extern uint8_t _gdt_start[];
extern uint8_t _gdt_end[];

/* Needs these for creating the task segment descriptor */
extern uint8_t _tss_start[];
extern uint8_t _tss_end[];

/**
 * Initialize and load the GDT.
 */
fernos_error_t init_gdt(void);
