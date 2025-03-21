
#pragma once

#include <stdint.h>
#include <stdint.h>

uint32_t read_eflags(void);

uint32_t read_cr0(void);

// Returns the value of the stack pointer
// before this function is called.
uint32_t *read_esp(void);

uint32_t read_cs(void);
uint32_t read_ds(void);

/**
 * This function just stops the computer basically.
 * YOU CANNOT RECOVER FROM THIS.
 */
void lock_up(void);
