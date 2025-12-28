
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
     * The buffer itself with size `pitch * height`.
     */
    gfx_color_t *buffer;
} gfx_buffer_t;

/**
 * Set all pixels in the given buffer to `color`.
 */
void gfx_clear(gfx_buffer_t *buf, gfx_color_t color);

/**
 * A view will be a rectangular section of a graphics buffer!
 */
typedef struct _gfx_view_t {
    /**
     * Allocator of this view. (Doesn't need to be used for views which are statically allocated)
     */
    allocator_t *al;
    
    gfx_buffer_t *parent_buffer;

    /*
     * The top left hand corner of this view will live at (`origin_x`, `origin_y`)
     * in the parent buffer.
     *
     * NOTE: These are SIGNED integers as it is completely valid for this view to 
     * begin/extend off screen.
     */

    int32_t origin_x;
    int32_t origin_y;

    uint32_t width;
    uint32_t height;
} gfx_view_t;

gfx_view_t *new_gfx_view(allocator_t *al, gfx_buffer_t *pb, int32_t x, int32_t y, uint32_t w, uint32_t h);
static inline gfx_view_t *new_da_gfx_view(gfx_buffer_t *pb, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    return new_gfx_view(get_default_allocator(), pb, x, y, w, h);
}
void delete_gfx_view(gfx_view_t *view);

/**
 * Fill a rectangle within a view.
 *
 * `(x, y)` is the top left corner of the rectangle relative to the view origin.
 */
void gfxv_fill_rect(gfx_view_t *view, int32_t x, int32_t y, uint32_t w, uint32_t h, gfx_color_t color);
