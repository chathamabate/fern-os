
#pragma once

#include "k_sys/dt.h"
#include "k_sys/gdt.h"

#define NUM_GDT_ENTRIES 0x10

/*
 * NOTE: IT IS GAURANTEED that the created GDT follows the following simple
 * structure.
 */

#define KERNEL_CODE_SELECTOR  ( 0x8 | ROOT_PRVLG)
#define KERNEL_DATA_SELECTOR  (0x10 | ROOT_PRVLG)
#define USER_CODE_SELECTOR    (0x18 | USER_PRVLG)
#define USER_DATA_SELECTOR    (0x20 | USER_PRVLG)
#define TSS_SELECTOR           0x28

extern uint8_t _gdt_start[];
extern uint8_t _gdt_end[];

/* Needs these for creating the task segment descriptor */
extern uint8_t _tss_start[];
extern uint8_t _tss_end[];

/**
 * Initialize and load the GDT.
 */
fernos_error_t init_gdt(void);
