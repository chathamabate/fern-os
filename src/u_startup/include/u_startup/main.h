#pragma once

#include <stdint.h>

extern uint8_t __tmp_stack[0x1000];

/**
 * Entry point for the first user process.
 */
void user_main(void);

