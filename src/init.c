

// Stuff called before main.

#include "msys/dt.h"
#include "msys/intr.h"
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

extern gate_descriptor_t _idt_start[];
extern char _idt_end;

uint32_t init_idt(void) {
    gate_descriptor_t NOP_GD = 
        gd_from_parts(0x8, (uint32_t)nop_handler, 0x8E);

    uint32_t len = 64; 

    for (uint32_t i = 0; i < len; i++) {
        _idt_start[i] = NOP_GD;
    }

    _idt_start[0x5]  =
        gd_from_parts(0x8, (uint32_t)my_handler, 0x8E);
    
    return (len * sizeof(gate_descriptor_t)) - 1;
}


