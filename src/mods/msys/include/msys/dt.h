

#ifndef MSYS_DT_H
#define MSYS_DT_H

#include <stdint.h>

struct _gdtr_val_t {
    uint16_t size_m_1; // size of the table (in bytes) - 1.
    uint32_t offset;
} __attribute__ ((packed));

typedef struct _gdtr_val_t gdtr_val_t;

typedef uint64_t seg_descriptor_t;

static inline uint32_t sd_get_limit(seg_descriptor_t sd) {
    return (sd & 0xFFFF) | ((sd >> 32) & 0x000F0000);
}

static inline uint32_t sd_get_base(seg_descriptor_t sd) {
    return ((sd >> 16) & 0xFFFFFF) | 
        ((sd >> 32) & 0xFF000000);
}

static inline uint8_t sd_get_flags(seg_descriptor_t sd) {
    return (sd >> 52) & 0xF;
}

static inline uint8_t sd_get_access_byte(seg_descriptor_t sd) {
    return (sd >> 40) & 0xFF;
}

static inline seg_descriptor_t sd_from_parts(uint32_t base, uint32_t limit, 
        uint8_t access_byte, uint8_t flags) {
    return 
        (limit & 0xFFFF) |
        ((((uint64_t)base) & 0xFFFFFF) << 16) |
        (((uint64_t)access_byte) << 40) |
        ((((uint64_t)limit) & 0x000F0000) << 32) |
        ((((uint64_t)flags) & 0xF) << 52) |
        ((((uint64_t)base) & 0xFF000000) << 32);
}

// Stores contents of the gdtr into where dest points.
void read_gdtr(gdtr_val_t *dest);

#endif
