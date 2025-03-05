
#ifndef MSYS_DEBUG_H
#define MSYS_DEBUG_H

#include <stdint.h>

// Returns the value of the stack pointer
// before this function is called.
uint32_t *read_esp(void);

uint32_t read_cs(void);
uint32_t read_ds(void);

// This function just stops the computer basically.
// YOU CANNOT RECOVER FROM THIS.
void lock_up(void);

#endif
