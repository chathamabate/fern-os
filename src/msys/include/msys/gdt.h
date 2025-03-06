
#pragma once

#include <stdint.h>

#define _TO_MASK(wid) ((1L << wid) - 1)

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
#define SD_LIMIT_LOWER_WID_MASK _TO_MASK(SD_LIMIT_LOWER_WID)
#define SD_LIMIT_LOWER_MASK (SD_LIMIT_LOWER_WID_MASK << SD_LIMIT_LOWER_OFF)       

// base_lower [16 : 39]
#define SD_BASE_LOWER_OFF (16)
#define SD_BASE_LOWER_WID (24)
#define SD_BASE_LOWER_WID_MASK _TO_MASK(SD_BASE_LOWER_WID)
#define SD_BASE_LOWER_MASK (SD_BASE_LOWER_WID_MASK << SD_BASE_LOWER_OFF)

// type [40 : 44]
#define SD_TYPE_OFF (40)
#define SD_TYPE_WID (5)
#define SD_TYPE_WID_MASK _TO_MASK(SD_TYPE_WID)
#define SD_TYPE_MASK (SD_TYPE_WID_MASK << SD_TYPE_OFF)

// privelege [45 : 46]
#define SD_PRIVILEGE_OFF (45)
#define SD_PRIVILEGE_WID (2)
#define SD_PRIVILEGE_WID_MASK _TO_MASK(SD_PRIVILEGE_WID)
#define SD_PRIVILEGE_MASK (SD_PRIVILEGE_WID_MASK << SD_PRIVILEGE_OFF)

// present [47]
#define SD_PRESENT_OFF (47)
#define SD_PRESENT_WID (1)
#define SD_PRESENT_WID_MASK _TO_MASK(SD_PRESENT_WID)
#define SD_PRESENT_MASK (SD_PRESENT_WID_MASK << SD_PRESENT_OFF)
                                            
// limit_upper [48 : 51]
#define SD_LIMIT_UPPER_OFF (48)
#define SD_LIMIT_UPPER_WID (4)
#define SD_LIMIT_UPPER_WID_MASK _TO_MASK(SD_LIMIT_UPPER_WID)
#define SD_LIMIT_UPPER_MASK (SD_LIMIT_UPPER_WID_MASK << SD_LIMIT_UPPER_OFF)   

// Available bit [52 : 52]
#define SD_AVAIL_OFF (52)
#define SD_AVAIL_WID (1)
#define SD_AVAIL_WID_MASK _TO_MASK(SD_AVAIL_WID)
#define SD_AVAIL_MASK (SD_AVAIL_WID_MASK << SD_AVAIL_OFF)

// zero        [53 : 53]
// ...         [54 : 54] // Specific to selector being used.

// granularity [55 : 55]
#define SD_GRAN_OFF (55)
#define SD_GRAN_WID (1)
#define SD_GRAN_WID_MASK _TO_MASK(SD_GRAN_WID)
#define SD_GRAN_MASK (SD_GRAN_WID_MASK << SD_GRAN_OFF)

// base_upper [56 : 63]
#define SD_BASE_UPPER_OFF (56)
#define SD_BASE_UPPER_WID (8)
#define SD_BASE_UPPER_WID_MASK _TO_MASK(SD_BASE_UPPER_WID)
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
#define DSD_ACCESSED_WID_MASK _TO_MASK(DSD_ACCESSED_WID)
#define DSD_ACCESSED_MASK (DSD_ACCESSED_WID_MASK << DSD_ACCESSED_OFF)       

// writable    [41 : 41]
#define DSD_WRITABLE_OFF (41)
#define DSD_WRITABLE_WID (1)  
#define DSD_WRITABLE_WID_MASK _TO_MASK(DSD_WRITABLE_WID)
#define DSD_WRITABLE_MASK (DSD_WRITABLE_WID_MASK << DSD_WRITABLE_OFF)       

// expand down [42 : 42]
#define DSD_EX_DOWN_OFF (42)
#define DSD_EX_DOWN_WID (1)  
#define DSD_EX_DOWN_WID_MASK _TO_MASK(DSD_EX_DOWN_WID)
#define DSD_EX_DOWN_MASK (DSD_EX_DOWN_WID_MASK << DSD_EX_DOWN_OFF)       

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

typedef seg_desc_t exec_seg_desc_t;

/*
struct _seg_desc_t {
    uint64_t limit_lower : 16;
    uint64_t base_lower : 24;

    uint64_t type : 5;

    uint64_t privilege : 2;
    uint64_t present : 1;

    uint64_t limit_upper : 4;
    
    uint64_t flags : 4;

    uint64_t base_upper : 8;
};

typedef struct _seg_desc_t seg_desc_t;

static inline seg_desc_t seg_desc(uint32_t base, uint32_t limit, uint32_t priv) {
    return (seg_desc_t) {
        .limit_lower = limit & 0xFFFF,
        .base_lower = base & 0xFFFFFF,

        .type = 0, // Fill this out yourself
        
        .privilege = priv & 0x3,
        .present = 0x1,

        .limit_upper = (limit & 0xF0000) >> 16,

        .flags = 0, // fill this out yourself

        .base_upper = (base & 0xFF000000) >> 24
    };
}

struct _data_seg_desc_t {
    uint64_t limit_lower : 16;
    uint64_t base_lower : 24;

    // -- Type --
    uint64_t accessed : 1;
    uint64_t writable : 1;
    uint64_t expand_down  : 1;

    uint64_t zero0 : 1; // Must be set as 0.
    uint64_t one0 : 1;  // Must be set as 1.
    // ----------

    uint64_t privilege : 2;
    uint64_t present : 1;

    uint64_t limit_upper : 4;

    // -- flags --
    uint64_t avail : 1;
    uint64_t zero1 : 1; // Must be set as 0.
    uint64_t big : 1;
    uint64_t granularity : 1;
    // ----------
    
    uint64_t base_upper : 8;
} __attribute__ ((packed));

typedef struct _data_seg_desc_t data_seg_desc_t;

static inline data_seg_desc_t data_seg_desc(uint32_t base, uint32_t limit, uint32_t priv) {
    data_seg_desc_t dsd = (data_seg_desc_t)seg_desc(base, limit, priv);
}

struct _exec_seg_desc_t {
    uint64_t limit_lower : 16;
    uint64_t base_lower : 24;

    // -- Type --
    uint64_t accessed : 1;
    uint64_t readable : 1;
    uint64_t conforming  : 1;

    uint64_t zero0 : 1; // Must be set as 0.
    uint64_t one0 : 1;  // Must be set as 1.
    // ----------

    uint64_t privilege : 2;
    uint64_t present : 1;

    uint64_t limit_upper : 4;

    // -- flags --
    uint64_t avail : 1;
    uint64_t zero1 : 1; // Must be set as 0.
    uint64_t def : 1;
    uint64_t granularity : 1;
    // ----------
    
    uint64_t base_upper : 8;
} __attribute__ ((packed));

typedef struct _exec_seg_desc_t exec_seg_desc_t;

struct _sys_seg_desc_t {
    uint64_t limit_lower : 16;
    uint64_t base_lower : 24;

    // -- Type --
    uint64_t type : 4;
    uint64_t zero0 : 1;  // Must be set as 1.
    // ----------

    uint64_t privilege : 2;
    uint64_t present : 1;

    uint64_t limit_upper : 4;

    // -- flags --
    uint64_t avail : 1;
    uint64_t zero1 : 1; // Must be set as 0.
    uint64_t _res0 : 1; // Reserved
    uint64_t granularity : 1;
    // ----------
    
    uint64_t base_upper : 8;
} __attribute__ ((packed));

typedef struct _sys_seg_desc_t sys_seg_desc_t;
*/

