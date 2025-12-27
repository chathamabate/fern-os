
#pragma once

#include <stdint.h>
#include "s_util/misc.h"

/*
 * Stateless Multiboot2 Structure Stuff
 *
 * NOTE: An "info tag" will be a tag which is within the information
 * block handed to the OS at boot time.
 * This is different from the Multiboot2 Header tags specified in the boot
 * assembly file. (Slightly confusing)
 */

/**
 * This number MUST be loaded into %eax when entering the kernel
 * to detect if multiboot2 tags were parsed successfully.
 */
#define M2_EAX_MAGIC (0x36D76289UL)

typedef struct _m2_info_start_t m2_info_start_t;
typedef struct _m2_info_tag_base_t m2_info_tag_base_t;

/**
 * Upon entering the kernel %ebx points to this structure!
 *
 * NOTE: This structure is assumed to always started at an 8-Byte aligned 
 * address.
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

/**
 * Get a pointer to the beginning of the tags.
 */
static inline const m2_info_tag_base_t *m2_info_tag_area(const m2_info_start_t *m2_is) {
    return (const m2_info_tag_base_t *)(m2_is + 1);
}

struct _m2_info_tag_base_t {
    /**
     * The type of this tag, 0 specifies the NULL tag!
     */
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

/* ITT for "info tag type" */

#define M2_ITT_NULL (0)

/**
 * Given an info tag, advance to the next.
 *
 * If the given info tag is the NULL tag, `it` is just returned as is.
 */
static inline const m2_info_tag_base_t *m2_next_info_tag(const m2_info_tag_base_t *it) {
    if (it->type == M2_ITT_NULL) {
        return it;
    }
    const uint8_t *next = ((const uint8_t *)it + it->size);
    if (!(IS_ALIGNED(next, 8))) {
        next += 8;
        next = (const uint8_t *)ALIGN(next, 8);
    }
    return (const m2_info_tag_base_t *)next;
}

#define M2_ITT_FRAMEBUFFER (8U)

/* FBT for frame buffer type */
#define M2_FBT_INDEXED_COLOR (0U)
#define M2_FBT_DIRECT_RGB    (1U)
#define M2_FBT_EGA_TEXT      (2U)

struct _m2_info_tag_framebuffer_t {
    m2_info_tag_base_t super;

    /**
     * Physical address of the framebuffer.
     */
    uint64_t addr;

    /**
     * Width of a row IN THE BUFFER! IN BYTES!
     */
    uint32_t pitch;

    /**
     * Number of visibile pixels in a row.
     */
    uint32_t width;

    /**
     * Number of rows on the screen.
     */
    uint32_t height;

    /**
     * BITS per pixel.
     */
    uint8_t bpp;

    /**
     * Framebuffer type.
     */
    uint8_t type;

    uint8_t reserved;

    // NOTE: The framebuffer tag usually will not end here, there will be
    // more bytes depending on the type of framebuffer which was installed!
} __attribute__ ((packed));

typedef struct _m2_info_tag_framebuffer_t m2_info_tag_framebuffer_t;

/**
 * For the DIRECT RGB type, this is what follows the frame buffer tag.
 * It specifies how to interpret the bytes per pixel quantity.
 */
struct _m2_framebuffer_rgb_spec_t {
    uint8_t r_field_pos;
    uint8_t r_mask_size;

    uint8_t g_field_pos;
    uint8_t g_mask_size;

    uint8_t b_field_pos;
    uint8_t b_mask_size;
} __attribute__ ((packed));

typedef struct _m2_framebuffer_rgb_spec_t m2_framebuffer_rgb_spec_t;

/**
 * Get a pointer to the rgb specification within a framebuffer tag.
 *
 * Returns NULL if the given framebuffer tag doesn't have the direct rgb type
 * specified.
 */
static inline const m2_framebuffer_rgb_spec_t *m2_framebuffer_get_rgb_spec(const m2_info_tag_framebuffer_t *it_fb) {
    if (it_fb->type == M2_FBT_DIRECT_RGB) {
        return (const m2_framebuffer_rgb_spec_t *)(it_fb + 1);
    }
    return NULL;
}


