
#ifndef MSYS_INTR_H
#define MSYS_INTR_H

// Here are the standard exception types:

#include <stdint.h>
#include <stdbool.h>

static inline bool exception_has_err_code(uint8_t intr) {
    return 
        intr == 0x8 ||
        (0xA <= intr && intr <= 0xE) ||
        intr == 0x11 || 
        intr == 0x15 ||
        intr == 0x1D ||
        intr == 0x1E;
}

void default_exception_handler(void);
void default_exception_with_err_code_handler(void);
void default_handler(void);

void enable_intrs(void);
void disable_intrs(void);

#endif
