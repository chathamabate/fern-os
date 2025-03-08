
#pragma once

#include <stdint.h>
#include "fstndutil/misc.h"


typedef uint64_t seg_desc_t;

// Generic Segmet Descriptor Format :
//
// limit_lower [ 0 : 15]
// base_lower  [16 : 39]
// type        [40 : 44] // Specific to selector being used.
// privilege   [45 : 46]
// present     [47 : 47]
// limit_upper [48 : 51]
// avail       [52 : 52]
// zero        [53 : 53]
// ...         [54 : 54] // Specific to selector being used.
// granularity [55 : 55]
// base_uper   [56 : 63]

// limit_lower [0 : 15]
#define SD_LIMIT_LOWER_OFF (0)      
#define SD_LIMIT_LOWER_WID (16)  
#define SD_LIMIT_LOWER_WID_MASK TO_MASK64(SD_LIMIT_LOWER_WID)
#define SD_LIMIT_LOWER_MASK (SD_LIMIT_LOWER_WID_MASK << SD_LIMIT_LOWER_OFF)       

// base_lower [16 : 39]
#define SD_BASE_LOWER_OFF (16)
#define SD_BASE_LOWER_WID (24)
#define SD_BASE_LOWER_WID_MASK TO_MASK64(SD_BASE_LOWER_WID)
#define SD_BASE_LOWER_MASK (SD_BASE_LOWER_WID_MASK << SD_BASE_LOWER_OFF)

// type [40 : 44]
#define SD_TYPE_OFF (40)
#define SD_TYPE_WID (5)
#define SD_TYPE_WID_MASK TO_MASK64(SD_TYPE_WID)
#define SD_TYPE_MASK (SD_TYPE_WID_MASK << SD_TYPE_OFF)

// privelege [45 : 46]
#define SD_PRIVILEGE_OFF (45)
#define SD_PRIVILEGE_WID (2)
#define SD_PRIVILEGE_WID_MASK TO_MASK64(SD_PRIVILEGE_WID)
#define SD_PRIVILEGE_MASK (SD_PRIVILEGE_WID_MASK << SD_PRIVILEGE_OFF)

// present [47]
#define SD_PRESENT_OFF (47)
#define SD_PRESENT_WID (1)
#define SD_PRESENT_WID_MASK TO_MASK64(SD_PRESENT_WID)
#define SD_PRESENT_MASK (SD_PRESENT_WID_MASK << SD_PRESENT_OFF)
                                            
// limit_upper [48 : 51]
#define SD_LIMIT_UPPER_OFF (48)
#define SD_LIMIT_UPPER_WID (4)
#define SD_LIMIT_UPPER_WID_MASK TO_MASK64(SD_LIMIT_UPPER_WID)
#define SD_LIMIT_UPPER_MASK (SD_LIMIT_UPPER_WID_MASK << SD_LIMIT_UPPER_OFF)   

// Available bit [52 : 52]
#define SD_AVAIL_OFF (52)
#define SD_AVAIL_WID (1)
#define SD_AVAIL_WID_MASK TO_MASK64(SD_AVAIL_WID)
#define SD_AVAIL_MASK (SD_AVAIL_WID_MASK << SD_AVAIL_OFF)

// zero        [53 : 53]
// ...         [54 : 54] // Specific to selector being used.

// granularity [55 : 55]
#define SD_GRAN_OFF (55)
#define SD_GRAN_WID (1)
#define SD_GRAN_WID_MASK TO_MASK64(SD_GRAN_WID)
#define SD_GRAN_MASK (SD_GRAN_WID_MASK << SD_GRAN_OFF)

// base_upper [56 : 63]
#define SD_BASE_UPPER_OFF (56)
#define SD_BASE_UPPER_WID (8)
#define SD_BASE_UPPER_WID_MASK TO_MASK64(SD_BASE_UPPER_WID)
#define SD_BASE_UPPER_MASK (SD_BASE_UPPER_WID_MASK << SD_BASE_UPPER_OFF)   

static inline seg_desc_t not_present_desc(void) {
    return 0;
}

static inline void sd_set_limit(seg_desc_t *sd, uint32_t limit) {
    seg_desc_t d = *sd;

    // Clear limit bits.
    d &= ~(SD_LIMIT_LOWER_MASK);
    d &= ~(SD_LIMIT_UPPER_MASK);

    // Set limit bits.
    d |= (SD_LIMIT_LOWER_WID_MASK & limit) << SD_LIMIT_LOWER_OFF;
    d |= (((SD_LIMIT_UPPER_WID_MASK << SD_LIMIT_LOWER_WID) & limit) >> SD_LIMIT_LOWER_WID) << SD_LIMIT_UPPER_OFF;

    *sd = d;
}

static inline uint32_t sd_get_limit(seg_desc_t sd) {
    return (
        ((SD_LIMIT_LOWER_MASK & sd) >> SD_LIMIT_LOWER_OFF) |
        (((SD_LIMIT_UPPER_MASK & sd) >> SD_LIMIT_UPPER_OFF) << SD_LIMIT_LOWER_WID)
    );
}

static inline void sd_set_base(seg_desc_t *sd, uint32_t base) {
    seg_desc_t d = *sd;
    
    // Clear base bits.
    d &= ~(SD_BASE_LOWER_MASK);
    d &= ~(SD_BASE_UPPER_MASK);

    // Set base bits.
    d |= (SD_BASE_LOWER_WID_MASK & base) << SD_BASE_LOWER_OFF;
    d |= (((SD_BASE_UPPER_WID_MASK << SD_BASE_LOWER_WID) & base) >> SD_BASE_LOWER_WID) << SD_BASE_UPPER_OFF;
 
    *sd = d;
}

static inline uint32_t sd_get_base(seg_desc_t sd) {
    return (
        ((SD_BASE_LOWER_MASK & sd) >> SD_BASE_LOWER_OFF) |
        (((SD_BASE_UPPER_MASK & sd) >> SD_BASE_UPPER_OFF) << SD_BASE_LOWER_WID)
    );
}

static inline void sd_set_type(seg_desc_t *sd, uint8_t type) {
    seg_desc_t d = *sd;

    d &= ~(SD_TYPE_MASK);
    d |= (SD_TYPE_WID_MASK & type) << SD_TYPE_OFF;

    *sd = d;
}

static inline uint8_t sd_get_type(seg_desc_t sd) {
    return (sd & SD_TYPE_MASK) >> SD_TYPE_OFF;
}

static inline void sd_set_privilege(seg_desc_t *sd, uint8_t priv) {
    seg_desc_t d = *sd;

    d &= ~(SD_PRIVILEGE_MASK);
    d |= (SD_PRIVILEGE_WID_MASK & priv) << SD_PRIVILEGE_OFF;

    *sd = d;
}

static inline uint8_t sd_get_privilege(seg_desc_t sd) {
    return (SD_PRIVILEGE_MASK & sd) >> SD_PRIVILEGE_OFF;
}

static inline void sd_set_present(seg_desc_t *sd, uint8_t p) {
    seg_desc_t d = *sd;

    d &= ~(SD_PRESENT_MASK);
    d |= (SD_PRESENT_WID_MASK & p) << SD_PRESENT_OFF;

    *sd = d;
}

static inline uint8_t sd_get_present(seg_desc_t sd) {
    return (SD_PRESENT_MASK & sd) >> SD_PRESENT_OFF;
}

static inline void sd_set_avail(seg_desc_t *sd, uint8_t p) {
    seg_desc_t d = *sd;

    d &= ~(SD_AVAIL_MASK);
    d |= (SD_AVAIL_WID_MASK & p) << SD_AVAIL_OFF;

    *sd = d;
}

static inline uint8_t sd_get_avail(seg_desc_t sd) {
    return (SD_AVAIL_MASK & sd) >> SD_AVAIL_OFF;
}

static inline void sd_set_gran(seg_desc_t *sd, uint8_t p) {
    seg_desc_t d = *sd;

    d &= ~(SD_GRAN_MASK);
    d |= (SD_GRAN_WID_MASK & p) << SD_GRAN_OFF;

    *sd = d;
}

static inline uint8_t sd_get_gran(seg_desc_t sd) {
    return (SD_GRAN_MASK & sd) >> SD_GRAN_OFF;
}

typedef seg_desc_t data_seg_desc_t;

// Data Descriptor Format :
// ...
// type        [40 : 44] 
//    accessed    [40 : 40]
//    writable    [41 : 41]
//    expand down [42 : 42]
//    zero        [43 : 43]
//    one         [44 : 44]
// ...
// big         [54 : 54] 
// ...

// accessed    [40 : 40]
#define DSD_ACCESSED_OFF (40)
#define DSD_ACCESSED_WID (1)  
#define DSD_ACCESSED_WID_MASK TO_MASK64(DSD_ACCESSED_WID)
#define DSD_ACCESSED_MASK (DSD_ACCESSED_WID_MASK << DSD_ACCESSED_OFF)       

// writable    [41 : 41]
#define DSD_WRITABLE_OFF (41)
#define DSD_WRITABLE_WID (1)  
#define DSD_WRITABLE_WID_MASK TO_MASK64(DSD_WRITABLE_WID)
#define DSD_WRITABLE_MASK (DSD_WRITABLE_WID_MASK << DSD_WRITABLE_OFF)       

// expand down [42 : 42]
#define DSD_EX_DOWN_OFF (42)
#define DSD_EX_DOWN_WID (1)  
#define DSD_EX_DOWN_WID_MASK TO_MASK64(DSD_EX_DOWN_WID)
#define DSD_EX_DOWN_MASK (DSD_EX_DOWN_WID_MASK << DSD_EX_DOWN_OFF)       

// big [54 : 54]
#define DSD_BIG_OFF (54)
#define DSD_BIG_WID (1)  
#define DSD_BIG_WID_MASK TO_MASK64(DSD_BIG_WID)
#define DSD_BIG_MASK (DSD_BIG_WID_MASK << DSD_BIG_OFF)       

static inline data_seg_desc_t data_seg_desc(void) {
    data_seg_desc_t dsd = not_present_desc();

    sd_set_type(&dsd, 0x10); // 0b10000 for data seg.
    sd_set_present(&dsd, 1); // Set as present.

    return dsd;
}

static inline void dsd_set_accessed(data_seg_desc_t *dsd, uint8_t accessed) {
    data_seg_desc_t d = *dsd;

    d &= ~(DSD_ACCESSED_MASK);
    d |= (DSD_ACCESSED_WID_MASK & accessed) << DSD_ACCESSED_OFF;

    *dsd = d;
}

static inline uint8_t dsd_get_accessed(data_seg_desc_t dsd) {
    return (DSD_ACCESSED_MASK & dsd) >> DSD_ACCESSED_OFF;
}

static inline void dsd_set_writable(data_seg_desc_t *dsd, uint8_t writable) {
    data_seg_desc_t d = *dsd;

    d &= ~(DSD_WRITABLE_MASK);
    d |= (DSD_WRITABLE_WID_MASK & writable) << DSD_WRITABLE_OFF;

    *dsd = d;
}

static inline uint8_t dsd_get_writable(data_seg_desc_t dsd) {
    return (DSD_WRITABLE_MASK & dsd) >> DSD_WRITABLE_OFF;
}

static inline void dsd_set_ex_down(data_seg_desc_t *dsd, uint8_t ex_down) {
    data_seg_desc_t d = *dsd;

    d &= ~(DSD_EX_DOWN_MASK);
    d |= (DSD_EX_DOWN_WID_MASK & ex_down) << DSD_EX_DOWN_OFF;

    *dsd = d;
}

static inline uint8_t dsd_get_ex_down(data_seg_desc_t dsd) {
    return (DSD_EX_DOWN_MASK & dsd) >> DSD_EX_DOWN_OFF;
}

static inline void dsd_set_big(data_seg_desc_t *dsd, uint8_t big) {
    data_seg_desc_t d = *dsd;

    d &= ~(DSD_BIG_MASK);
    d |= (DSD_BIG_WID_MASK & big) << DSD_BIG_OFF;

    *dsd = d;
}

static inline uint8_t dsd_get_big(data_seg_desc_t dsd) {
    return (DSD_BIG_MASK & dsd) >> DSD_BIG_OFF;
}

typedef seg_desc_t exec_seg_desc_t;

// Code Descriptor Format :
// ...
// type        [40 : 44] 
//    accessed    [40 : 40]
//    readable    [41 : 41]
//    conforming  [42 : 42]
//    one         [43 : 43]
//    one         [44 : 44]
// ...
// default     [54 : 54] 
// ...

// accessed    [40 : 40]
#define ESD_ACCESSED_OFF (40)
#define ESD_ACCESSED_WID (1)  
#define ESD_ACCESSED_WID_MASK TO_MASK64(ESD_ACCESSED_WID)
#define ESD_ACCESSED_MASK (ESD_ACCESSED_WID_MASK << ESD_ACCESSED_OFF)       

// readable    [41 : 41]
#define ESD_READABLE_OFF (41)
#define ESD_READABLE_WID (1)  
#define ESD_READABLE_WID_MASK TO_MASK64(ESD_READABLE_WID)
#define ESD_READABLE_MASK (ESD_READABLE_WID_MASK << ESD_READABLE_OFF)       

// conforming [42 : 42]
#define ESD_CONFORMING_OFF (42)
#define ESD_CONFORMING_WID (1)  
#define ESD_CONFORMING_WID_MASK TO_MASK64(ESD_CONFORMING_WID)
#define ESD_CONFORMING_MASK (ESD_CONFORMING_WID_MASK << ESD_CONFORMING_OFF)       

// default [54 : 54]
#define ESD_DEFAULT_OFF (54)
#define ESD_DEFAULT_WID (1)  
#define ESD_DEFAULT_WID_MASK TO_MASK64(ESD_DEFAULT_WID)
#define ESD_DEFAULT_MASK (ESD_DEFAULT_WID_MASK << ESD_DEFAULT_OFF)       

static inline exec_seg_desc_t exec_seg_desc(void) {
    exec_seg_desc_t esd = not_present_desc();

    sd_set_type(&esd, 0x18); // 0b11000 for exec seg.
    sd_set_present(&esd, 1); // Set as present.

    return esd;
}

static inline void esd_set_accessed(data_seg_desc_t *esd, uint8_t accessed) {
    data_seg_desc_t d = *esd;

    d &= ~(ESD_ACCESSED_MASK);
    d |= (ESD_ACCESSED_WID_MASK & accessed) << ESD_ACCESSED_OFF;

    *esd = d;
}

static inline uint8_t esd_get_accessed(data_seg_desc_t esd) {
    return (ESD_ACCESSED_MASK & esd) >> ESD_ACCESSED_OFF;
}

static inline void esd_set_readable(data_seg_desc_t *esd, uint8_t readable) {
    data_seg_desc_t d = *esd;

    d &= ~(ESD_READABLE_MASK);
    d |= (ESD_READABLE_WID_MASK & readable) << ESD_READABLE_OFF;

    *esd = d;
}

static inline uint8_t esd_get_readable(data_seg_desc_t esd) {
    return (ESD_READABLE_MASK & esd) >> ESD_READABLE_OFF;
}

static inline void esd_set_conforming(data_seg_desc_t *esd, uint8_t conforming) {
    data_seg_desc_t d = *esd;

    d &= ~(ESD_CONFORMING_MASK);
    d |= (ESD_CONFORMING_WID_MASK & conforming) << ESD_CONFORMING_OFF;

    *esd = d;
}

static inline uint8_t esd_get_conforming(data_seg_desc_t esd) {
    return (ESD_CONFORMING_MASK & esd) >> ESD_CONFORMING_OFF;
}

static inline void esd_set_def(data_seg_desc_t *esd, uint8_t def) {
    data_seg_desc_t d = *esd;

    d &= ~(ESD_DEFAULT_MASK);
    d |= (ESD_DEFAULT_WID_MASK & def) << ESD_DEFAULT_OFF;

    *esd = d;
}

static inline uint8_t esd_get_def(data_seg_desc_t esd) {
    return (ESD_DEFAULT_MASK & esd) >> ESD_DEFAULT_OFF;
}

/*
 * TODO: Add system segment descriptors.
 */

// GDTR Work.

// The GDTR Register has the following format:
// size   [0  : 15] (size of the table - 1 in bytes)
// offset [16 : 47] (physical address of where the table begins)
//
// NOTE: The register only holds 48 bits, we'll abstract it as 64 bits here in C.
typedef uint64_t gdtr_val_t;

#define GDTV_SIZE_OFF (0)      
#define GDTV_SIZE_WID (16)  
#define GDTV_SIZE_WID_MASK TO_MASK64(GDTV_SIZE_WID)
#define GDTV_SIZE_MASK (GDTV_SIZE_WID_MASK << GDTV_SIZE_OFF)       

#define GDTV_BASE_OFF (16)      
#define GDTV_BASE_WID (32)  
#define GDTV_BASE_WID_MASK TO_MASK64(GDTV_BASE_WID)
#define GDTV_BASE_MASK (GDTV_BASE_WID_MASK << GDTV_BASE_OFF)       

static inline gdtr_val_t gdtr_val(void) {
    return 0;
}

static inline uint32_t gdtv_get_size(gdtr_val_t gdtv) {
    return ((gdtv & GDTV_SIZE_MASK) >> GDTV_SIZE_OFF) + 1;
}

static inline void gdtv_set_size(gdtr_val_t *gdtv, uint32_t size) {
    gdtr_val_t g = *gdtv;

    g &= ~(GDTV_SIZE_MASK);
    g |= ((size - 1) & GDTV_SIZE_WID_MASK) << GDTV_SIZE_OFF;

    *gdtv = g;
}

static inline uint32_t gdtv_get_num_entries(gdtr_val_t gdtv) {
    return gdtv_get_size(gdtv) / sizeof(seg_desc_t);
}

static inline void gdtv_set_num_entries(gdtr_val_t *gdtv, uint32_t num_entries) {
    gdtv_set_size(gdtv, num_entries * sizeof(seg_desc_t));
}

// NOTE: These two function freak out clangd for some reason, they are correct though.

static inline seg_desc_t *gdtv_get_base(gdtr_val_t gdtv) {
    return (seg_desc_t *)(uint32_t)((gdtv & GDTV_BASE_MASK) >> GDTV_BASE_OFF);
}

static inline void gdtv_set_base(gdtr_val_t *gdtv, seg_desc_t *base) {
    gdtr_val_t g = *gdtv;

    g &= ~(GDTV_BASE_MASK);
    g |= ((uint32_t)base & GDTV_BASE_WID_MASK) << GDTV_BASE_OFF;

    *gdtv = g;
}

gdtr_val_t read_gdtr(void);

// NOTE: After doing the register load, cs will be loaded with 0x08, and all other
// segment registers will be set with 0x10.
void load_gdtr(gdtr_val_t v);
