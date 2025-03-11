#pragma once

#include "s_util/misc.h"
#include <stdint.h>

// Descriptor table register work.
//
// Applies to both idtr and gdtr
//
// This assumes all descriptors are 8 bytes.

// The DTR Register has the following format:
// size   [0  : 15] (size of the table - 1 in bytes)
// offset [16 : 47] (linear address of where the table begins)
//
// NOTE: The register only holds 48 bits, we'll abstract it as 64 bits here in C.
typedef uint64_t dtr_val_t;

#define DTV_SIZE_OFF (0)      
#define DTV_SIZE_WID (16)  
#define DTV_SIZE_WID_MASK TO_MASK64(DTV_SIZE_WID)
#define DTV_SIZE_MASK (DTV_SIZE_WID_MASK << DTV_SIZE_OFF)       

#define DTV_BASE_OFF (16)      
#define DTV_BASE_WID (32)  
#define DTV_BASE_WID_MASK TO_MASK64(DTV_BASE_WID)
#define DTV_BASE_MASK (DTV_BASE_WID_MASK << DTV_BASE_OFF)       

static inline dtr_val_t dtr_val(void) {
    return 0;
}

static inline uint32_t dtv_get_size(dtr_val_t dtv) {
    return ((dtv & DTV_SIZE_MASK) >> DTV_SIZE_OFF) + 1;
}

static inline void dtv_set_size(dtr_val_t *dtv, uint32_t size) {
    dtr_val_t g = *dtv;

    g &= ~(DTV_SIZE_MASK);
    g |= ((size - 1) & DTV_SIZE_WID_MASK) << DTV_SIZE_OFF;

    *dtv = g;
}

static inline uint32_t dtv_get_num_entries(dtr_val_t dtv) {
    return dtv_get_size(dtv) / 8;
}

static inline void dtv_set_num_entries(dtr_val_t *dtv, uint32_t num_entries) {
    dtv_set_size(dtv, num_entries * 8);
}

// NOTE: These two function freak out clangd for some reason, they are correct though.

static inline void *dtv_get_base(dtr_val_t dtv) {
    return (void *)(uint32_t)((dtv & DTV_BASE_MASK) >> DTV_BASE_OFF);
}

static inline void dtv_set_base(dtr_val_t *dtv, void *base) {
    dtr_val_t g = *dtv;

    g &= ~(DTV_BASE_MASK);
    g |= ((uint32_t)base & DTV_BASE_WID_MASK) << DTV_BASE_OFF;

    *dtv = g;
}
