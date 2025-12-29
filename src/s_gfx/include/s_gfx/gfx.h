
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "s_mem/allocator.h"

/*
 * NOTE: This GFX module works exclusively with 32-bit
 * color graphics!
 */

typedef uint32_t gfx_color_t;

static inline gfx_color_t gfx_color(uint8_t r, uint8_t g, uint8_t b) {
    return (1 << 24) | (r << 16UL) | (g << 8UL) | (b << 0UL);
}

/**
 * A color value which is clear!
 */
#define GFX_COLOR_CLEAR (0UL)

/**
 * Whether or not a color is clear.
 */
static inline bool gfx_color_is_clear(gfx_color_t color) {
    return (color & 0xFF000000) == 0;
}

typedef struct _gfx_buffer_t {
    /**
     * Bytes per row in the buffer! (must be non-negative)
     */
    int32_t pitch;

    /**
     * Visible pixels per row. (must be non-negative)
     */
    int32_t width;

    /**
     * Number of rows. (must be non-negative)
     */
    int32_t height;

    /**
     * The buffer itself with size `pitch * height`.
     */
    gfx_color_t *buffer;
} gfx_buffer_t;

/**
 * Set all pixels in the given buffer to `color`.
 */
void gfx_clear(gfx_buffer_t *buf, gfx_color_t color);


/**
 * Fill a rectangle within a view.
 *
 * `(x, y)` is the top left corner of the rectangle relative to the view origin.
 * `w` and `h` must both be non-negative.
 */
void gfx_fill_rect(gfx_buffer_t *buf, int32_t x, int32_t y, int32_t w, int32_t h, gfx_color_t color);

/**
 * Fill a monochrome bitmap on the screen?
 */
void gfx_fill_bitmap(gfx_buffer_t *buf, int32_t x, int32_t y, uint8_t w_scale, uint8_t h_scale,
        uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, gfx_color_t fg_color, gfx_color_t bg_color);


