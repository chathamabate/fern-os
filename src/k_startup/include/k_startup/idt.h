
#pragma once

#include "k_sys/dt.h"
#include "k_sys/idt.h"

#define NUM_IDT_ENTRIES 0x100

/**
 * The interrupt descriptor table.
 */
extern seg_desc_t idt[NUM_IDT_ENTRIES];

/**
 * Initialize and load the interrupt descriptor table.
 */
fernos_error_t init_idt(void);
