
#pragma once

#include "k_sys/dt.h"
#include "k_sys/gdt.h"

#define NUM_GDT_ENTRIES 0x10

/**
 * The Global descriptor table.
 */
extern seg_desc_t gdt[NUM_GDT_ENTRIES];

/**
 * Initialize and load the GDT.
 */
fernos_error_t init_gdt(void);
