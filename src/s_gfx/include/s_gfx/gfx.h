
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

/*
 * NOTE: Below we deal heavily with integer coordinates.
 * Integers are subject to wrapping when values become too large or small.
 *
 * For most cases, we will assume undefined behavior in the case of wrapping.
 * If using a reasonably sized buffer and only attempting reasonable rendering,
 * wrapping will never occur. Checking for wraps everywhere can be confusing
 * and slow!
 */

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
    allocator_t * const al;

    /**
     * Dimmensions of the buffer in pixels.
     */
    uint16_t width, height;

    /**
     * The buffer itself with size `width * height * sizeof(gfx_color_t)`.
     */
    gfx_color_t *buffer;
} gfx_buffer_t;

/**
 * Create a new dynamically allocated gfx buffer with dimmensions w x h.
 *
 * Returns NULL on error.
 */
gfx_buffer_t *new_gfx_buffer(allocator_t *al, uint16_t w, uint16_t h);

static inline gfx_buffer_t *new_da_gfx_buffer(uint16_t w, uint16_t h) {
    return new_gfx_buffer(get_default_allocator(), w, h);
}

/**
 * Delete a gfx buffer. 
 * (This does nothing if the allocator of `buf` is NULL)
 */
void delete_gfx_buffer(gfx_buffer_t *buf);

/**
 * Attempt to resize `buf`.
 * 
 * If `shrink` is false, then resizing to a smaller cumulative size will do nothing memory wise.
 * If `shrink` is true, resizing to a smaller cumulative size may shrink the buffer in memory.
 *
 * If `buf` has no allocator, this always fails.
 * Otherwise, this fails if there are insufficient resources.
 *
 * On success, the contents of the buffer or undefined.
 * On failure, the buffer is left unmodified.
 */
fernos_error_t gfx_resize_buffer(gfx_buffer_t *buf, uint16_t w, uint16_t h, bool shrink);

/**
 * Set all pixels in the given buffer to `color`.
 */
void gfx_clear(gfx_buffer_t *buf, gfx_color_t color);

/**
 * A box
 */
typedef struct _gfx_box_t {
    /**
     * Top left corner of the box.
     */
    int32_t x, y;

    /**
     * Dimmensions of the box.
     *
     * `x + width <= INT32_MAX` and `y + height <= INT32_MAX`.
     * Otherwise, undefined behavior!
     */
    uint16_t width, height;
} gfx_box_t;

/**
 * Clip the given box to the clip area!
 * `box` is modified to be within the `clip_area` if possible.
 *
 * If `box` and `clip_area` don't intersect, false is returned!
 * In this case `box` is left unmodified.
 * 
 * On intersection, true is returned.
 */
bool gfx_clip(const gfx_box_t *clip_area, gfx_box_t *box);

/**
 * Clip `box` to the `clip_area` and then the buffer!
 *
 * Realize that a `clip_area` isn't necessarily within a buffer's bounds.
 * Thus, this functions is helpful for conforming a box to both a clip
 * area and a buffer!
 *
 * NOTE: `clip_area` can be NULL!
 */
bool gfx_clip_with_buffer(const gfx_buffer_t *buf, 
        const gfx_box_t *clip_area, gfx_box_t *box);

/**
 * Fill a box on screen. (relative to top left corner of buffer)
 *
 * `clip_area` is optional.
 */
void gfx_fill_box(gfx_buffer_t *buf, const gfx_box_t *clip_area, 
        const gfx_box_t *box, gfx_color_t color);

/**
 * Fill a rectangle within the buffer.
 *
 * `(x, y)` is the top left corner of the rectangle relative to the buffer origin.
 * `w` and `h` are the dimmensions of the rectangle being filled.
 * `x + w <= INT32_MAX` and `y + h <= INT32_MAX` or else undefined behavior!
 * `color` is the color to fill the rectangle with.
 *
 * NOTE: `clip_area` is an optional argument, if non-null, only pixels within the clipped area
 * are able to be drawn to!
 */
static inline void gfx_fill_rect(gfx_buffer_t *buf, const gfx_box_t *clip_area,
        int32_t x, int32_t y, uint16_t w, uint16_t h, gfx_color_t color) {
    gfx_box_t box = {
        .x = x, .y = y, .width = w, .height = h
    };
    gfx_fill_box(buf, clip_area, &box, color);
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
void gfx_fill_bitmap(gfx_buffer_t *buf, const gfx_box_t *clip_area,
        int32_t x, int32_t y, uint8_t w_scale, uint8_t h_scale,
        const uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, gfx_color_t fg_color, gfx_color_t bg_color);

/**
 * Paste the contents of one buffer into another.
 *
 * This is a straight copy/paste, no notion of "clear" pixels.
 */
void gfx_paste_buffer(gfx_buffer_t *buf, const gfx_box_t *clip_area,
    const gfx_buffer_t *sub_buf, int32_t x, int32_t y);


