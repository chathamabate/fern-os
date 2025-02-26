

// Stuff called before main.

#include "msys/dt.h"
#include "msys/intr.h"
#include "terminal/out.h"
#include "util/str.h"
#include <stdint.h>

#define NUM_GDT_ENTRIES 0x10
static seg_descriptor_t gdt[NUM_GDT_ENTRIES] __attribute__ ((aligned(8)));

void gdt_init(void) {
    gdt[0] = sd_from_parts(0x0, 0x0, 0x0, 0x0);
    gdt[1] = sd_from_parts(
            0x0, 0xFFFFF, 

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
            0x0, 0xFFFFF, 

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
            0x0, 0xFFFFF, 

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
          
    gdt[4] = sd_from_parts(
            0x0, 0xFFFFF, 

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
    
    for (uint32_t i = 5; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = sd_from_parts(0x0, 0x0, 0x0, 0x0);
    }

    load_gdtr(gdt, sizeof(gdt) - 1, 0x10);
}

#define NUM_IDT_ENTRIES 0x100
static gate_descriptor_t idt[NUM_IDT_ENTRIES] __attribute__ ((aligned(8)));

void idt_init(void) {
    uint8_t len = 64; 

    // Program all exceptions.
    for (uint32_t i = 0; i < 32; i++) {
        idt[i] = gd_from_parts(
            0x8, 
            (uint32_t)(exception_has_err_code(i) 
                ? default_exception_with_err_code_handler 
                : default_exception_handler),
            IDT_ATTR_32b_INTR_GATE |
            IDT_ATTR_ROOT_PRVLG |
            IDT_ATTR_PRESENT
        );
    }

    for (uint32_t i = 32; i < NUM_IDT_ENTRIES; i++) {
        idt[i] = gd_from_parts(
            0x8, 
            (uint32_t)default_handler, 
            IDT_ATTR_32b_INTR_GATE |
            IDT_ATTR_ROOT_PRVLG |
            IDT_ATTR_PRESENT
        );
    }

    idt[0x8] =  gd_from_parts(
        0x8, 
        (uint32_t)nop_exception_handler, 
        IDT_ATTR_32b_INTR_GATE |
        IDT_ATTR_ROOT_PRVLG |
        IDT_ATTR_PRESENT
    );


    load_idtr(idt, sizeof(idt) - 1);
}

void paging_init(void) {

}


