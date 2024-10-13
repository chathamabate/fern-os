

// Stuff called before main.

#include "msys/dt.h"
#include <stdint.h>

extern seg_descriptor_t _gdt_start[];
extern char _gdt_end;

uint32_t init_flat_gdt(void) {
    seg_descriptor_t *gdt =_gdt_start;  
    gdt[0] = sd_from_parts(0x0, 0x0, 0x0, 0x0);
    gdt[1] = sd_from_parts(0x0, 0xFFFFF, 0x9A, 0xC);    // Kernel Code. 0x8     (DON"T CHANGE)
    gdt[2] = sd_from_parts(0x0, 0xFFFFF, 0x92, 0xC);    // Kernel Data. 0x10    (DON'T CHANGE)
    gdt[3] = sd_from_parts(0x0, 0xFFFFF, 0xFA, 0xC);    // User Code.   0x18
    gdt[4] = sd_from_parts(0x0, 0xFFFFF, 0xF2, 0xC);    // User Data.   0x20
    
    return (5 * sizeof(seg_descriptor_t)) - 1;
}
