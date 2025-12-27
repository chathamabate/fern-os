
#pragma once

#include <stdint.h>

/*
 * Stateless Multiboot2 Structure Stuff
 */

/**
 * This number MUST be loaded into %eax when entering the kernel
 * to detect if multiboot2 tags were parsed successfully.
 */
#define M2_EAX_MAGIC (0x36D76289UL)

/**
 * Upon entering the kernel %ebx points to this structure!
 */
struct _m2_info_start_t {
    /**
     * This includes the size of this structure.
     * The size of all boot info tags cumulatively,
     * (including the null tag at the end).
     */
    uint32_t total_size;
    uint32_t reserved;

    // Followed by boot info tags!
} __attribute__ ((packed));

typedef struct _m2_info_start_t m2_info_start_t;

struct _m2_info_tag_base_t {
    uint32_t type;

    /**
     * Size of this tag, (including this structure)
     *
     * VERY IMPORTANT!
     * All tags are 8-byte aligned in memory.
     * So, sometimes, padding might come after a tag.
     * This `size` field DOES NOT include padding.
     *
     * When iterating over all info tags, make sure to take into
     * account padding!
     */
    uint32_t size;
} __attribute__ ((packed));

typedef struct _m2_info_tag_base_t m2_info_tag_base_t;

struct _m2_info_tag_framebuffer_t {

    uint8_t dummy;

} __attribute__ ((packed));

typedef struct _m2_info_tag_framebuffer_t m2_info_tag_framebuffer_t;
