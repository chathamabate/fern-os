

#ifndef MSYS_GDT_H
#define MSYS_GDT_H

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
    return ((sd >> 16) & 0xFFFF) | 
        ((sd >> 16) & 0x00FF0000) |
        ((sd >> 32) & 0xFF000000);
}

static inline uint8_t sd_get_flags(seg_descriptor_t sd) {
    return (sd >> 52) & 0xF;
}

static inline uint8_t sd_get_access_byte(seg_descriptor_t sd) {
    return (sd >> 40) & 0xFF;
}

// Stores contents of the gdtr into where dest points.
// NOTE: This will write 
void read_gdtr(gdtr_val_t *dest);

#endif
