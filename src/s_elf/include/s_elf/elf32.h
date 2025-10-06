
#pragma once

#include <stdint.h>

typedef struct _elf32_header_t elf32_header_t;
typedef struct _elf32_program_header_t elf32_program_header_t;
typedef struct _elf32_section_header_t elf32_section_header_t;

/**
 * In little endian this should be 
 * 0x7F, 'E', 'L', 'F'.
 */
#define ELF_HEADER_MAGIC (0x464C457FU)

struct _elf32_header_t {
    /**
     * 4-Byte Header magic value.
     */
    uint32_t header_magic;

    /**
     * Should have value 1 for 32-bit.
     */
    uint8_t cls;

    /**
     * Should have value 1 for little endian.
     */
    uint8_t endian;

    /**
     * Should be set to 1 always for some reason?
     */
    uint8_t version0;

    /**
     * Should maybe be 0xFF, signifying this ELF is not for a well known operating system.
     * (I'll likely ignore this field)
     */
    uint8_t os_abi;

    /**
     * Further context for `os_abi`.
     */
    uint8_t os_abi_version;

    /**
     * All 0s.
     */
    uint8_t pad0[7];

    /**
     * The type of object held in this ELF file.
     */
    uint16_t obj_type;

    /**
     * Target Architecture.
     *
     * Should be 0x03 for x86 architecture.
     */
    uint16_t machine;

    /**
     * Also set to 1 for original version of the elf.
     */
    uint32_t version1;

    /**
     * Entry point of the ELF File.
     */
    uint32_t entry;

    /**
     * The start of the program header table.
     * (Likely directly after this header at 0x34)
     */
    uint32_t program_header_table;

    /**
     * The start of the section header table.
     */
    uint32_t section_header_table;

    /**
     * Architecture specific flags.
     */
    uint32_t flags;

    /**
     * Size of this header. Should be 52 for 32-bit.
     */
    uint16_t this_header_size;

    /**
     * Size of a program header. Should be 0x20 for 32-bit.
     */
    uint16_t program_header_size;

    /**
     * Number of program headers.
     */
    uint16_t num_program_headers;

    /**
     * Size of a section header. Should be 0x28 for 32-bit.
     */
    uint16_t section_header_size;

    /**
     * Number of section headers.
     */
    uint16_t num_section_headers;

    /**
     * Index into the section headers table of the section header which contains section
     * names.
     */
    uint16_t section_names_header_index;
} __attribute__ ((packed));

typedef uint32_t elf32_segment_type_t;

#define ELF32_SEG_TYPE_NULL       (0UL)
#define ELF32_SEG_TYPE_LOADABLE   (1UL)
#define ELF32_SEG_TYPE_DYNAMIC    (2UL)
#define ELF32_SEG_TYPE_INTERP     (3UL)
#define ELF32_SEG_TYPE_NOTE       (4UL)
#define ELF32_SEG_TYPE_SHLIB      (5UL)
#define ELF32_SEG_TYPE_HDR_TBL    (6UL)
#define ELF32_SEG_TYPE_TLS        (7UL)
// There are more, but I think these are the only ones we'll ever care about.


typedef uint32_t elf32_segment_flags_t;

#define ELF32_SEG_FLAG_EXECUTABLE (1UL << 0)
#define ELF32_SEG_FLAG_WRITEABLE  (1UL << 1)
#define ELF32_SEG_FLAG_READABLE   (1UL << 2)

/**
 * A program header describes a segment of data within the ELF file.
 */
struct _elf32_program_header_t {
    /**
     * The type of the segment this header points to.
     */
    elf32_segment_type_t type;

    /**
     * Offset into the ELF file of the segment this header describes.
     */
    uint32_t offset;

    /**
     * Virtual address for this segment to be loaded to.
     */
    void *vaddr;

    /**
     * Physical address for this segment. (We will likely not use this)
     */
    uint32_t paddr;

    /**
     * Size of the corresponding segment in the ELF file. (Can be 0)
     */
    uint32_t size_in_file;

    /**
     * Size of the segment in memory. (Can be 0)
     */
    uint32_t size_in_mem;

    /**
     * Flags for this segment.
     */
    elf32_segment_flags_t flags;

    /**
     * 0 or 1 mean No alignment.
     *
     * Otherwise a power of 2.
     */
    uint32_t align;
} __attribute__ ((packed));

struct _elf32_section_header_t {
    

} __attribute__ ((packed));
