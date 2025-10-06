
#pragma once

#include <stdint.h>

typedef struct _elf32_header_t elf32_header_t;

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
