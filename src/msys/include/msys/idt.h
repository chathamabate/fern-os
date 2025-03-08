
#pragma once

#include <stdint.h>
#include "fstndutil/misc.h"

typedef uint64_t gate_desc_t;

// Gate Descriptor Format : 
//
// ...
// selector  [16 : 31]
// ... 
// type      [48 : 52]
// privilege [53 : 54]
// present   [55 : 55]
// ...

#define GD_SELECTOR_OFF (16)      
#define GD_SELECTOR_WID (16)  
#define GD_SELECTOR_WID_MASK TO_MASK64(GD_SELECTOR_WID)
#define GD_SELECTOR_MASK (GD_SELECTOR_WID_MASK << GD_SELECTOR_OFF)       

#define GD_TYPE_OFF (48)      
#define GD_TYPE_WID (5)  
#define GD_TYPE_WID_MASK TO_MASK64(GD_TYPE_WID)
#define GD_TYPE_MASK (GD_TYPE_WID_MASK << GD_TYPE_OFF)       

#define GD_PRIVILEGE_OFF (53)      
#define GD_PRIVILEGE_WID (2)  
#define GD_PRIVILEGE_WID_MASK TO_MASK64(GD_PRIVILEGE_WID)
#define GD_PRIVILEGE_MASK (GD_PRIVILEGE_WID_MASK << GD_PRIVILEGE_OFF)       

#define GD_PRESENT_OFF (55)      
#define GD_PRESENT_WID (1)  
#define GD_PRESENT_WID_MASK TO_MASK64(GD_PRIVILEGE_WID)
#define GD_PRESENT_MASK (GD_PRIVILEGE_WID_MASK << GD_PRIVILEGE_OFF)       

static inline gate_desc_t not_present_gate_desc(void) {
    return 0;
}

static inline void gd_set_selector(gate_desc_t *gd, uint32_t selector) {
    gate_desc_t d = *gd;

    d &= ~(GD_SELECTOR_MASK);
    d |= (selector & GD_SELECTOR_WID_MASK) << GD_SELECTOR_OFF;

    *gd = d;
}

static inline uint32_t gd_get_selector(gate_desc_t gd) {
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

typedef gate_desc_t task_gate_desc_t;

// Format of task gate desc :
//
// type = 00101

static inline task_gate_desc_t task_gate_desc(void) {
    task_gate_desc_t tgd = not_present_gate_desc();

    gd_set_present(&tgd, 0x1);
    gd_set_type(&tgd, 0x5);

    return tgd;

}

typedef gate_desc_t intr_gate_desc_t;

// Format of interupt gate desc :
//
// offset_lower [0: 15]
//
// zeros        [36 : 39]
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
    gd_set_type(&igd, 0xE);

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
// zeros        [36 : 39]
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
    gd_set_type(&trgd, 0xF);

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
