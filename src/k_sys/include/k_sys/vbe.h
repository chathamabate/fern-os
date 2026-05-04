
#pragma once

#include <stdint.h>

typedef struct _vbe_far_ptr_t vbe_far_ptr_t;

struct _vbe_far_ptr_t {
    uint16_t offset;

    /**
     * This is a real mode selector. 
     */
    uint16_t real_sel;
} __attribute__ ((packed));

static inline void *vbe_far_to_protected_32_ptr(vbe_far_ptr_t fptr) {
    return  (void *)(((uint32_t)(fptr.real_sel) << 4) + fptr.offset);
}

typedef struct _vbe_info_block_t vbe_info_block_t;

struct _vbe_info_block_t {
    /**
     * Should be populated to "VESA" by function 00h
     */
    char sig[4];

    uint16_t version;

    vbe_far_ptr_t oem_str_ptr;

    uint32_t capabilities;
    vbe_far_ptr_t video_mode_ptr;

    /**
     * Number of 64kb memory blocks.
     */
    uint16_t total_memory;

    uint16_t oem_sw_revision;

    vbe_far_ptr_t oem_vendor_name_str;
    vbe_far_ptr_t oem_product_name_str;
    vbe_far_ptr_t oem_product_rev_str;
    
    /**
     * Where implementation specific information is placed!
     */
    union {
        uint8_t nop[222];
    } impl;

    uint8_t oem_data[256];
} __attribute__ ((packed));

void dump_vbe_info_block(const vbe_info_block_t *vib, void (*log)(const char *, ...));
