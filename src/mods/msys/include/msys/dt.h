

#ifndef MSYS_DT_H
#define MSYS_DT_H

#include <stdint.h>

// Global Descriptor Table Stuff

typedef uint64_t seg_descriptor_t;

struct _gdtr_val_t {
    uint16_t size_m_1; // size of the table (in bytes) - 1.
    seg_descriptor_t *offset;
} __attribute__ ((packed));

typedef struct _gdtr_val_t gdtr_val_t;

static inline uint32_t sd_get_limit(seg_descriptor_t sd) {
    return ((sd & 0xFFFF) | ((sd >> 32) & 0x000F0000));
}

static inline uint32_t sd_get_base(seg_descriptor_t sd) {
    return (((sd >> 16) & 0xFFFFFF) | 
        ((sd >> 32) & 0xFF000000));
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
        (((uint64_t)limit) & 0xFFFF) |
        ((((uint64_t)base) & 0xFFFFFF) << 16) |
        (((uint64_t)access_byte) << 40) |
        ((((uint64_t)limit) & 0x000F0000) << 32) |
        ((((uint64_t)flags) & 0xF) << 52) |
        ((((uint64_t)base) & 0xFF000000) << 32);
}

// Stores contents of the gdtr into where dest points.
void read_gdtr(gdtr_val_t *dest);


// Load a new GDT into the GDT register.
// NOTE: Make sure the current value of CS is valid in the new GDT.
// NOTE: After loading the GDT, 0x8 will be loaded into CS.
void load_gdtr(seg_descriptor_t *offset, uint32_t size_m_1, uint32_t new_data_seg);

// Interrupt Descriptor Table Stuff

typedef uint64_t gate_descriptor_t;

struct _idtr_val_t {
    uint16_t size_m_1;  // Size of table bytes - 1.
    gate_descriptor_t *offset;
} __attribute__ ((packed));

typedef struct _idtr_val_t idtr_val_t;

static inline uint16_t gd_get_selector(gate_descriptor_t gd) {
    return (gd >> 16) & 0xFFFF;
}

static inline uint32_t gd_get_offset(gate_descriptor_t gd) {
    return ((gd & 0xFFFF) | ((gd >> 32) & 0xFFFF0000));
}

static inline uint8_t gd_get_attrs(gate_descriptor_t gd) {
    return (gd >> 40) & 0xFF;
}

static inline gate_descriptor_t gd_from_parts(uint16_t selector, uint32_t offset, 
        uint8_t attrs) {
    return 
        (((uint64_t)offset) & 0xFFFF) |
        (((uint64_t)selector) << 16) |
        (((uint64_t)attrs) << 40) |
        ((((uint64_t)offset) & 0xFFFF0000) << 32);
}

void read_idtr(idtr_val_t *dest);
void load_idtr(gate_descriptor_t *offset, uint32_t size_m_1);

#endif
