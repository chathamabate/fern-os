

// Stuff called before main.

#include "msys/dt.h"
#include "msys/intr.h"
#include "terminal/out.h"
#include "util/str.h"
#include <stdint.h>

extern seg_descriptor_t _gdt_start[];
extern char _gdt_end;

void gdt_init(void) {
    seg_descriptor_t *gdt =_gdt_start;  
    gdt[0] = sd_from_parts(0x0, 0x0, 0x0, 0x0);
    gdt[1] = sd_from_parts(
            0x0, 0x0FFFF, 

            GDT_AB_PRESENT |
            GDT_AB_ROOT_PRVLG |
            GDT_AB_CODE_OR_DATA_SEG |
            GDT_AB_EXECUTABLE |
            GDT_AB_NON_CONFORMING |
            GDT_AB_READ |
            GDT_AB_ACCESSED,

            GDT_F_4K_GRANULARITY |
            GDT_F_32b_PROTECTED_MODE | 
            GDT_F_NON_LONG_MODE
    );    // Kernel Code. 0x8     
          
    gdt[2] = sd_from_parts(
            0x0, 0x0FFFF, 

            GDT_AB_PRESENT |
            GDT_AB_ROOT_PRVLG |
            GDT_AB_CODE_OR_DATA_SEG |
            GDT_AB_NOT_EXECUTABLE |
            GDT_AB_DIRECTION_UP |
            GDT_AB_WRITE |
            GDT_AB_ACCESSED,

            GDT_F_4K_GRANULARITY |
            GDT_F_32b_PROTECTED_MODE | 
            GDT_F_NON_LONG_MODE
    );    // Kernel Data. 0x10   
          
    gdt[3] = sd_from_parts(
            0x0, 0x0FFFF, 

            GDT_AB_PRESENT |
            GDT_AB_USER_PRVLG |
            GDT_AB_CODE_OR_DATA_SEG |
            GDT_AB_EXECUTABLE |
            GDT_AB_NON_CONFORMING |
            GDT_AB_READ |
            GDT_AB_ACCESSED,

            GDT_F_4K_GRANULARITY |
            GDT_F_32b_PROTECTED_MODE | 
            GDT_F_NON_LONG_MODE
    );    // User Code.   0x18
          
    gdt[4] = sd_from_parts(0x0, 0x0FFFF, 
            GDT_AB_PRESENT |
            GDT_AB_USER_PRVLG |
            GDT_AB_CODE_OR_DATA_SEG |
            GDT_AB_NOT_EXECUTABLE |
            GDT_AB_DIRECTION_UP |
            GDT_AB_WRITE |
            GDT_AB_ACCESSED,

            GDT_F_4K_GRANULARITY |
            GDT_F_32b_PROTECTED_MODE | 
            GDT_F_NON_LONG_MODE
    );    // User Data.   0x20
          //
    gdt[4] = sd_from_parts(0x10000000, 0xFFFFF, 
            GDT_AB_PRESENT |
            GDT_AB_ROOT_PRVLG |
            GDT_AB_CODE_OR_DATA_SEG |
            GDT_AB_NOT_EXECUTABLE |
            GDT_AB_DIRECTION_UP |
            GDT_AB_NO_WRITE |
            GDT_AB_ACCESSED,

            GDT_F_4K_GRANULARITY |
            GDT_F_32b_PROTECTED_MODE | 
            GDT_F_NON_LONG_MODE
    );    // User Data.   0x20
                                                        
    load_gdtr(_gdt_start, (5 * sizeof(seg_descriptor_t)) - 1, 0x10);
}

extern gate_descriptor_t _idt_start[];
extern char _idt_end;

extern void default_handler(void);

void idt_init(void) {
    gate_descriptor_t DEF_GD = gd_from_parts(
            0x8, 
            (uint32_t)default_handler,
            IDT_ATTR_32b_INTR_GATE |
            IDT_ATTR_ROOT_PRVLG |
            IDT_ATTR_PRESENT
    );
    // Doesn't do anything, but still potentially hazardous
    //gate_descriptor_t EMPTY_GD = gd_from_parts(
        //0x0, 0x0, IDT_ATTR_NOT_PRESENT
    //);

    uint32_t len = 64; 

    for (uint32_t i = 0; i < len; i++) {
        _idt_start[i] = DEF_GD;
    }

    load_idtr(_idt_start, (len * sizeof(gate_descriptor_t)) - 1);
}


