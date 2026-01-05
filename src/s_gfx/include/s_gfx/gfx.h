
#pragma once

#include <stdint.h>
#include <stddef.h>

#include "s_mem/allocator.h"

/**
 * This implementation of colors uses the following 32-bit scheme: 
 *
 * 0xAARRGGBB
 *
 * NOTE: The Alpha bits aren't really traditional, clean transparency isn't supported.
 * If the AA bits are non-zero, the color is 100% opaque.
 * If the AA bits are zero, the color is 100% transparent, (i.e. clear).
 *
 * 0x01010203 is the same color as 0xFF010203
 * 0x00010203 is NOT the same color as 0x01010203
 * 0x00010203 is the same color as 0x00FFFFFF (Both being the clear color)
 */
typedef uint32_t gfx_color_t;

/**
 * Create a color with 100% opacity.
 */
static inline gfx_color_t gfx_color(uint8_t r, uint8_t g, uint8_t b) {
    return (1 << 24) | (r << 16UL) | (g << 8UL) | (b << 0UL);
}

/**
 * Given two colors, determine if they are equal.
 */
bool gfx_color_equal(gfx_color_t c0, gfx_color_t c1);

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

/**
 * A gfx buffer is meant to be an intermediate target for graphics.
 * We expect the kernel to be able to take this type of structure and render it onto 
 * the visible screen somehow.
 */
typedef struct _gfx_buffer_t {
    /**
     * The allocator used to allocate this structure and `buffer`.
     *
     * This structure is intentially simple though to allow for 
     * easily setting up a buffer in static memory.
     */
    allocator_t *al;

    /**
     * Dimmensions of this buffer in pixels.
     */
    uint16_t width, height;

    /**
     * The buffer itself with size `width * height * sizeof(gfx_color_t)`.
     */
    gfx_color_t *buffer;
} gfx_buffer_t;

/**
 * Set all pixels in the given buffer to `color`.
 */
void gfx_clear(gfx_buffer_t *buf, gfx_color_t color);

/**
 * A rectangular area.
 */
typedef struct _gfx_box_t {
    int32_t x, y;
    uint16_t width, height;
} gfx_box_t;

/**
 * A way of styling what is to be rendered.
 */
typedef struct _gfx_style_t {
    /** x coordinates will be multiplied by this value */
    uint8_t x_scale;

    /** y coordinates will be multiplied by this value */
    uint8_t y_scale;

    /** widths will be multiplied by this value */
    uint8_t width_scale;

    /** heights will be multiplied by this value */
    uint8_t height_scale;

    /** Foreground color of what is to be rendered */
    gfx_color_t fg_color;

    /** Backgroudn color of what is to be rendered */
    gfx_color_t bg_color;
    
    /**
     * Only pixels in this area will be rendered.
     * (x, y) of the clip area is relative to the origin of whatever buffer is being drawn to.
     * When drawing to a buffer, coordinates should also be relative to the buffer, NOT this area.
     */
    gfx_box_t clip_area;
} gfx_style_t;

/**
 * Initialize a style to default values.
 *
 * The clip_area will just be as large as possible.
 */
void gfx_init_style(gfx_style_t *style);

/**
 * Fill a rectangle.
 *
 * `(x, y)` is the top left corner of the rectangle relative to the view origin.
 */
void gfxv_fill_rect(gfx_view_t *view, int32_t x, int32_t y, uint16_t w, uint16_t h);

/**
 * Fill an entire view with a given color.
 */
static inline void gfxv_clear(gfx_view_t *view, gfx_color_t color) {
    gfxv_fill_rect(view, 0, 0, view->width, view->height, color);
}

/**
 * Fill a rectangle relative to the buffer.
 */
static inline void gfx_fill_rect(gfx_buffer_t *buf, int32_t x, int32_t y, int32_t w, int32_t h,
        gfx_color_t color) {
    gfx_view_t view = {
        .buffer = buf, .x = 0, .y = 0, .width = buf->width, .height = buf->height,
    };
    gfxv_fill_rect(&view, x, y, w, h, color);
}


/**
 * Fill a monochrome bitmap on the screen.
 *
 * `(x, y)` will be where the top left corner of the bitmap will begin on screen.
 * `(w_scale, h_scale)` will be by how much to scale the bitmap by during render.
 *
 * `bitmap` is the bitmap to be rendered where 1's represent pixels to be rendered with 
 * `fg_color` and 0's represent pixels to be rendered with `bg_color`.
 *
 * `bitmap` will be rendered within a rectangle with dimmensions 
 * `(w_scale * bitmap_cols) x (h_scale * bitmap_rows)` pixels.
 *
 * `bitmap` should have length `bitmap_rows * round_up(bitmap_cols, 8)`.
 * If a bitmap row only needs 11 bits for example, 
 * the bitmap should have 16 bits (2 bytes) for each row where the final 5 bits of 
 * each row aren't rendered.
 */
void gfx_fill_bitmap(gfx_buffer_t *buf, int32_t x, int32_t y, uint8_t w_scale, uint8_t h_scale,
        const uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, gfx_color_t fg_color, gfx_color_t bg_color);


