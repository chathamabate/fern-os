
#pragma once

#include <stdint.h>

/*
 * NOTE: This GFX module works exclusively with 32-bit
 * color graphics!
 */

typedef uint32_t gfx_color_t;

static inline gfx_color_t gfx_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16UL) | (g << 8UL) | (b << 0UL);
}

typedef struct _gfx_buffer_t {
    /**
     * Bytes per row in the buffer!
     */
    uint32_t pitch;

    /**
     * Visible pixels per row.
     */
    uint16_t width;

    /**
     * Number of rows.
     */
    uint16_t height;

    /**
     * 
     */
    gfx_color_t *buffer;
} gfx_buffer_t;
