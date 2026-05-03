
#pragma once

#include <stdint.h>


typedef struct _vbe_info_block_t vbe_info_block_t;

struct _vbe_info_block_t {
    /**
     * Should be populated to "VESA" by function 00h
     */
    char sig[4];

    uint16_t version;

    uint32_t oem_str_ptr;
    uint32_t capabilities;
    uint32_t video_mode_ptr;

    /**
     * Number of 64kb memory blocks.
     */
    uint16_t total_memory;

    uint16_t oem_sw_revision;

    uint32_t oem_vendor_name_str;
    uint32_t oem_product_name_str;
    uint32_t oem_product_rev_str;
} __attribute__ ((packed));
