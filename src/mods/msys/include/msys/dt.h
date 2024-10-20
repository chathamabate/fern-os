

#ifndef MSYS_DT_H
#define MSYS_DT_H

#include <stdint.h>
#include <stddef.h>

// Global Descriptor Table Stuff

struct _seg_descriptor_t {
    uint64_t limit_lo : 16;
    uint64_t base_lo : 24;
    uint64_t access_byte : 8;
    uint64_t limit_hi : 4;
    uint64_t flags : 4;
    uint64_t base_hi : 8;
} __attribute__ ((packed));

typedef struct _seg_descriptor_t seg_descriptor_t;

struct _gdtr_val_t {
    uint16_t size_m_1; // size of the table (in bytes) - 1.
    seg_descriptor_t *offset;
} __attribute__ ((packed));

typedef struct _gdtr_val_t gdtr_val_t;

// Access Byte Definition.

#define GDT_AB_P_MASK (1 << 7)
#define GDT_AB_DPL_MASK (3 << 5)
#define GDT_AB_S_MASK (1 << 4)
#define GDT_AB_E_MASK (1 << 3)
#define GDT_AB_DC_MASK (1 << 2)
#define GDT_AB_RW_MASK (1 << 1)
#define GDT_AB_A_MASK (1 << 0)

// Present Bit
#define GDT_AB_PRESENT GDT_AB_P_MASK
#define GDT_AB_NOT_PRESENT 0x0

// DPL Bits
#define GDT_AB_USER_PRVLG GDT_AB_DPL_MASK
#define GDT_AB_ROOT_PRVLG 0x0

// Descriptor Type Bit
#define GDT_AB_TSS 0x0
#define GDT_AB_CODE_OR_DATA_SEG GDT_AB_S_MASK

// Executable Bit
#define GDT_AB_EXECUTABLE GDT_AB_E_MASK
#define GDT_AB_NOT_EXECUTABLE 0x0

// Direction Bit/Conforming Bit
#define GDT_AB_DIRECTION_UP 0x0             // For Data Segments
#define GDT_AB_DIRECTION_DOWN GDT_AB_DC_MASK
#define GDT_AB_NON_CONFORMING 0x0           // For Code Segments
#define GDT_AB_CONFOMRING GDT_AB_DC_MASK

// RW Bit
#define GDT_AB_WRITE GDT_AB_RW_MASK // For Data Segments
#define GDT_AB_NO_WRITE 0x0
#define GDT_AB_READ GDT_AB_RW_MASK  // For Code Segments
#define GDT_AB_NO_READ 0x0

// Accessed Bit
#define GDT_AB_ACCESSED GDT_AB_A_MASK
#define GDT_AB_NOT_ACCESSED 0x0

//  Flag Byte definitions.

#define GDT_F_G_MASK (1 << 3)
#define GDT_F_DB_MASK (1 << 2)
#define GDT_F_L_MASK (1 << 1)

// Granularity Bit.
#define GDT_F_1B_GRANULARITY 0x0
#define GDT_F_4K_GRANULARITY GDT_F_G_MASK

// Size Bit.
#define GDT_F_16b_PROTECTED_MODE 0x0
#define GDT_F_32b_PROTECTED_MODE GDT_F_DB_MASK

// Long Mode Bit.
#define GDT_F_LONG_MODE GDT_F_L_MASK
#define GDT_F_NON_LONG_MODE 0x0

static inline uint32_t sd_get_limit(seg_descriptor_t sd) {
    return sd.limit_lo | (sd.limit_hi << 16);
}

static inline uint32_t sd_get_base(seg_descriptor_t sd) {
    return sd.base_lo | (sd.base_hi << 24);
}

static inline uint8_t sd_get_flags(seg_descriptor_t sd) {
    return sd.flags;
}

static inline uint8_t sd_get_access_byte(seg_descriptor_t sd) {
    return sd.access_byte;
}

static inline seg_descriptor_t sd_from_parts(uint32_t base, uint32_t limit, 
        uint8_t access_byte, uint8_t flags) {
    seg_descriptor_t sd = {
        .limit_lo = limit & 0xFFFF,
        .base_lo = base & 0xFFFFFF,
        .access_byte = access_byte,
        .limit_hi = (limit & 0xF0000) >> 16,
        .flags = flags,
        .base_hi = (base & 0xFF000000) >> 24
    };

    return sd;
}

// Stores contents of the gdtr into where dest points.
void read_gdtr(gdtr_val_t *dest);


// Load a new GDT into the GDT register.
// NOTE: Make sure the current value of CS is valid in the new GDT.
// NOTE: After loading the GDT, 0x8 will be loaded into CS.
void load_gdtr(seg_descriptor_t *offset, uint32_t size_m_1, uint32_t new_data_seg);

// Interrupt Descriptor Table Stuff

struct _gate_descriptor_t {
    uint64_t offset_lo : 16;
    uint64_t seg_selector : 16;
    uint64_t reserved : 8;
    uint64_t attrs : 8;
    uint64_t offset_hi : 16; 
} __attribute__ ((packed));

typedef struct _gate_descriptor_t gate_descriptor_t;

struct _idtr_val_t {
    uint16_t size_m_1;  // Size of table bytes - 1.
    gate_descriptor_t *offset;
} __attribute__ ((packed));

typedef struct _idtr_val_t idtr_val_t;

#define IDT_ATTR_GT_MASK 0xF
#define IDT_ATTR_DPL_MASK (3 << 5)
#define IDT_ATTR_P_MASK (1 << 7)

// Gate Type
#define IDT_ATTR_TASK_GATE 0x5
#define IDT_ATTR_16b_INTR_GATE 0x6
#define IDT_ATTR_16b_TRAP_GATE 0x7
#define IDT_ATTR_32b_INTR_GATE 0xE
#define IDT_ATTR_32b_TRAP_GATE 0xF

// Privlege Level
#define IDT_ATTR_ROOT_PRVLG 0x0
#define IDT_ATTR_USER_PRVLG IDT_ATTR_DPL_MASK

// Present Bit
#define IDT_ATTR_PRESENT IDT_ATTR_P_MASK
#define IDT_ATTR_NOT_PRESENT 0x0

static inline uint16_t gd_get_selector(gate_descriptor_t gd) {
    return gd.seg_selector >> 16;
}

static inline uint32_t gd_get_offset(gate_descriptor_t gd) {
    return gd.offset_lo | (gd.offset_hi << 16);
}

static inline uint8_t gd_get_attrs(gate_descriptor_t gd) {
    return gd.attrs;
}

static inline gate_descriptor_t gd_from_parts(uint16_t selector, uint32_t offset, 
        uint8_t attrs) {
    gate_descriptor_t gd = {
        .offset_lo = offset & 0xFFFF,
        .seg_selector = selector,
        .reserved = 0,
        .attrs = attrs,
        .offset_hi = (offset >> 16)
    };

    return gd;
}

void read_idtr(idtr_val_t *dest);
void load_idtr(gate_descriptor_t *offset, uint32_t size_m_1);

void load_gate_descriptor(size_t i, gate_descriptor_t gd);

#endif
