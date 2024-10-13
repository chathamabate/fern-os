
#ifndef MSYS_IO_H
#define MSYS_IO_H

#include <stdint.h>

// The value should be a byte, and the port a word.
// I am using 32-bits for both arguments since this routine
// is written in assembly. Want to make sure these are always passed
// on the stack in 4-bytes chunks.
void outb(uint32_t port, uint32_t value);

void enable_intrs(void);
void disable_intrs(void);

#endif
