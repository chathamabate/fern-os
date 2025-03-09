
#pragma once

#include <stdint.h>
#include "fstndutil/misc.h"
#include "msys/dt.h"
#include "msys/gdt.h"

typedef uint64_t gate_desc_t;

// Gate Descriptor Format : 
//
// ...
// selector  [16 : 31]
// ... 
// type      [40 : 44]
// privilege [45 : 46]
// present   [47 : 47]
// ...

#define GD_SELECTOR_OFF (16)      
#define GD_SELECTOR_WID (16)  
#define GD_SELECTOR_WID_MASK TO_MASK64(GD_SELECTOR_WID)
#define GD_SELECTOR_MASK (GD_SELECTOR_WID_MASK << GD_SELECTOR_OFF)       

#define GD_TYPE_OFF (40)      
#define GD_TYPE_WID (4)  
#define GD_TYPE_WID_MASK TO_MASK64(GD_TYPE_WID)
#define GD_TYPE_MASK (GD_TYPE_WID_MASK << GD_TYPE_OFF)       

#define GD_PRIVILEGE_OFF (45)      
#define GD_PRIVILEGE_WID (2)  
#define GD_PRIVILEGE_WID_MASK TO_MASK64(GD_PRIVILEGE_WID)
#define GD_PRIVILEGE_MASK (GD_PRIVILEGE_WID_MASK << GD_PRIVILEGE_OFF)       

#define GD_PRESENT_OFF (47)      
#define GD_PRESENT_WID (1)  
#define GD_PRESENT_WID_MASK TO_MASK64(GD_PRESENT_WID)
#define GD_PRESENT_MASK (GD_PRESENT_WID_MASK << GD_PRESENT_OFF)       

static inline gate_desc_t not_present_gate_desc(void) {
    return 0;
}

static inline void gd_set_selector(gate_desc_t *gd, seg_selector_t selector) {
    gate_desc_t d = *gd;

    d &= ~(GD_SELECTOR_MASK);
    d |= (selector & GD_SELECTOR_WID_MASK) << GD_SELECTOR_OFF;

    *gd = d;
}

static inline seg_selector_t gd_get_selector(gate_desc_t gd) {
    return (gd & GD_SELECTOR_MASK) >> GD_SELECTOR_OFF;
}

static inline void gd_set_type(gate_desc_t *gd, uint32_t type) {
    gate_desc_t d = *gd;

    d &= ~(GD_TYPE_MASK);
    d |= (type & GD_TYPE_WID_MASK) << GD_TYPE_OFF;

    *gd = d;
}

static inline uint32_t gd_get_type(gate_desc_t gd) {
    return (gd & GD_TYPE_MASK) >> GD_TYPE_OFF;
}

static inline void gd_set_privilege(gate_desc_t *gd, uint32_t privilege) {
    gate_desc_t d = *gd;

    d &= ~(GD_PRIVILEGE_MASK);
    d |= (privilege & GD_PRIVILEGE_WID_MASK) << GD_PRIVILEGE_OFF;

    *gd = d;
}

static inline uint32_t gd_get_privilege(gate_desc_t gd) {
    return (gd & GD_PRIVILEGE_MASK) >> GD_PRIVILEGE_OFF;
}

static inline void gd_set_present(gate_desc_t *gd, uint8_t present) {
    gate_desc_t d = *gd;

    d &= ~(GD_PRESENT_MASK);
    d |= (present & GD_PRESENT_WID_MASK) << GD_PRESENT_OFF;

    *gd = d;
}

static inline uint8_t gd_get_present(gate_desc_t gd) {
    return (gd & GD_PRESENT_MASK) >> GD_PRESENT_OFF;
}

// Gate Descriptor Types

#define GD_TASK_GATE_TYPE (0x5)
#define GD_INTR_GATE_TYPE (0xE)
#define GD_TRAP_GATE_TYPE (0xF)

typedef gate_desc_t task_gate_desc_t;

// Format of task gate desc :
//
// type = 00101

static inline task_gate_desc_t task_gate_desc(void) {
    task_gate_desc_t tgd = not_present_gate_desc();

    gd_set_present(&tgd, 0x1);
    gd_set_type(&tgd, GD_TASK_GATE_TYPE);

    return tgd;

}

typedef gate_desc_t intr_gate_desc_t;

// Format of interupt gate desc :
//
// offset_lower [0: 15]
//
// zeros        [37 : 39]
// type = 01110
//
// offset_upper [48 : 63]
//

#define IGD_BASE_LOWER_OFF (0)      
#define IGD_BASE_LOWER_WID (16)  
#define IGD_BASE_LOWER_WID_MASK TO_MASK64(IGD_BASE_LOWER_WID)
#define IGD_BASE_LOWER_MASK (IGD_BASE_LOWER_WID_MASK << IGD_BASE_LOWER_OFF)       

#define IGD_BASE_UPPER_OFF (48)      
#define IGD_BASE_UPPER_WID (16)  
#define IGD_BASE_UPPER_WID_MASK TO_MASK64(IGD_BASE_UPPER_WID)
#define IGD_BASE_UPPER_MASK (IGD_BASE_UPPER_WID_MASK << IGD_BASE_UPPER_OFF)       

static inline intr_gate_desc_t intr_gate_desc(void) {
    intr_gate_desc_t igd = not_present_gate_desc(); 

    gd_set_present(&igd, 0x1);
    gd_set_type(&igd, GD_INTR_GATE_TYPE);

    return igd;
}

static inline void igd_set_base(intr_gate_desc_t *igd, uint32_t base) {
    intr_gate_desc_t gd = *igd;

    gd &= ~(IGD_BASE_LOWER_MASK);
    gd |= (base & IGD_BASE_LOWER_WID_MASK) << IGD_BASE_LOWER_OFF;

    gd &= ~(IGD_BASE_UPPER_MASK);
    gd |= ((base >> IGD_BASE_LOWER_WID) & IGD_BASE_UPPER_WID_MASK) << IGD_BASE_UPPER_OFF;

    *igd = gd;
}

static inline uint32_t igd_get_base(intr_gate_desc_t igd) {
    uint32_t base = 0;

    base |= (igd & IGD_BASE_LOWER_MASK) >> IGD_BASE_LOWER_OFF;
    base |= ((igd & IGD_BASE_UPPER_MASK) >> IGD_BASE_UPPER_OFF) << IGD_BASE_LOWER_WID;

    return base;
}

typedef gate_desc_t trap_gate_desc_t;

// Format of interupt gate desc :
//
// offset_lower [0: 15]
//
// zeros        [37 : 39]
// type = 01111
//
// offset_upper [48 : 63]
//

#define TRGD_BASE_LOWER_OFF (0)      
#define TRGD_BASE_LOWER_WID (16)  
#define TRGD_BASE_LOWER_WID_MASK TO_MASK64(TRGD_BASE_LOWER_WID)
#define TRGD_BASE_LOWER_MASK (TRGD_BASE_LOWER_WID_MASK << TRGD_BASE_LOWER_OFF)       

#define TRGD_BASE_UPPER_OFF (48)      
#define TRGD_BASE_UPPER_WID (16)  
#define TRGD_BASE_UPPER_WID_MASK TO_MASK64(TRGD_BASE_UPPER_WID)
#define TRGD_BASE_UPPER_MASK (TRGD_BASE_UPPER_WID_MASK << TRGD_BASE_UPPER_OFF)       

static inline trap_gate_desc_t trap_gate_desc(void) {
    trap_gate_desc_t trgd = not_present_gate_desc(); 

    gd_set_present(&trgd, 0x1);
    gd_set_type(&trgd, GD_TRAP_GATE_TYPE);

    return trgd;
}

static inline void trgd_set_base(trap_gate_desc_t *trgd, uint32_t base) {
    trap_gate_desc_t gd = *trgd;

    gd &= ~(TRGD_BASE_LOWER_MASK);
    gd |= (base & TRGD_BASE_LOWER_WID_MASK) << TRGD_BASE_LOWER_OFF;

    gd &= ~(TRGD_BASE_UPPER_MASK);
    gd |= ((base >> TRGD_BASE_LOWER_WID) & TRGD_BASE_UPPER_WID_MASK) << TRGD_BASE_UPPER_OFF;

    *trgd = gd;
}

static inline uint32_t trgd_get_base(trap_gate_desc_t trgd) {
    uint32_t base = 0;

    base |= (trgd & TRGD_BASE_LOWER_MASK) >> TRGD_BASE_LOWER_OFF;
    base |= ((trgd & TRGD_BASE_UPPER_MASK) >> TRGD_BASE_UPPER_OFF) << TRGD_BASE_LOWER_WID;

    return base;
}

// The below is basically a copy and paste of the gdtr register logic.

typedef uint64_t idtr_val_t;

#define IDTV_SIZE_OFF (0)      
#define IDTV_SIZE_WID (16)  
#define IDTV_SIZE_WID_MASK TO_MASK64(IDTV_SIZE_WID)
#define IDTV_SIZE_MASK (IDTV_SIZE_WID_MASK << IDTV_SIZE_OFF)       

#define IDTV_BASE_OFF (16)      
#define IDTV_BASE_WID (32)  
#define IDTV_BASE_WID_MASK TO_MASK64(IDTV_BASE_WID)
#define IDTV_BASE_MASK (IDTV_BASE_WID_MASK << IDTV_BASE_OFF)       

static inline idtr_val_t idtr_val(void) {
    return 0;
}

static inline uint32_t idtv_get_size(idtr_val_t idtv) {
    return ((idtv & IDTV_SIZE_MASK) >> IDTV_SIZE_OFF) + 1;
}

static inline void idtv_set_size(idtr_val_t *idtv, uint32_t size) {
    idtr_val_t g = *idtv;

    g &= ~(IDTV_SIZE_MASK);
    g |= ((size - 1) & IDTV_SIZE_WID_MASK) << IDTV_SIZE_OFF;

    *idtv = g;
}

static inline uint32_t idtv_get_num_entries(idtr_val_t idtv) {
    return idtv_get_size(idtv) / sizeof(gate_desc_t);
}

static inline void idtv_set_num_entries(idtr_val_t *idtv, uint32_t num_entries) {
    idtv_set_size(idtv, num_entries * sizeof(gate_desc_t));
}

// NOTE: These two function freak out clangd for some reason, they are correct though.

static inline gate_desc_t *idtv_get_base(idtr_val_t idtv) {
    return (gate_desc_t *)(uint32_t)((idtv & IDTV_BASE_MASK) >> IDTV_BASE_OFF);
}

static inline void idtv_set_base(idtr_val_t *idtv, gate_desc_t *base) {
    idtr_val_t g = *idtv;

    g &= ~(IDTV_BASE_MASK);
    g |= ((uint32_t)base & IDTV_BASE_WID_MASK) << IDTV_BASE_OFF;

    *idtv = g;
}

dtr_val_t read_idtr(void);
void load_idtr(dtr_val_t idtv);

