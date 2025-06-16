
#pragma once

#include <stdint.h>

/**
 * The value should be a byte, and the port a word.
 * I am using 32-bits for both arguments since this routine
 * is written in assembly. Want to make sure these are always passed
 * on the stack in 4-bytes chunks.
 */
void outb(uint32_t port, uint32_t value);

uint8_t inb(uint32_t port);

/**
 * Out a word. Value will be interpreted as a 16-bit value.
 */
void outw(uint32_t port, uint32_t value);

uint16_t inw(uint32_t port);

static inline void io_wait(void) {
    outb(0x80, 0x0);
}

static inline void outb_and_wait(uint32_t port, uint32_t value) {
    outb(port, value);
    io_wait();
}

