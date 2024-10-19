
#ifndef MSYS_DEBUG_H
#define MSYS_DEBUG_H

// Returns the value of the stack pointer
// before this function is called.
uint32_t *read_esp(void);

uint32_t read_cs(void);
uint32_t read_ds(void);

#endif
