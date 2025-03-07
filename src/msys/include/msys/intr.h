
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "msys/debug.h"

void enable_intrs(void);
void disable_intrs(void);

static inline uint32_t intrs_enabled(void) {
    return (read_eflags() & (1 << 9)) != 0;
}

// The idea of an "interrupt section" is to restore the correct value
// of the interrupt enable flag after leaving the section.

static inline uint32_t intr_section_enter(void) {
    uint32_t en = intrs_enabled();
    disable_intrs();
    return en;
}

static inline void intr_section_exit(uint32_t en) {
    if (en) {
        enable_intrs();
    }
}



