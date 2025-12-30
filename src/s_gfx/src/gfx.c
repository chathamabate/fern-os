
#include "s_gfx/gfx.h"
#include <limits.h>
#include "s_util/misc.h"

#define GFX_COLOR_ALPHA_MASK (0xFF000000UL)

bool gfx_color_equal(gfx_color_t c0, gfx_color_t c1) {
    if ((c0 & GFX_COLOR_ALPHA_MASK) || (c1 & GFX_COLOR_ALPHA_MASK)) {
        return (c0 & GFX_COLOR_ALPHA_MASK) && (c1 & GFX_COLOR_ALPHA_MASK);
    }
    return (c0 & ~GFX_COLOR_ALPHA_MASK) == (c1 & ~GFX_COLOR_ALPHA_MASK);
}

/**
 * Return a pointer to row `row` within `buf`.
 *
 * THIS DOES NO CHECKS ON `row` !
 */
static gfx_color_t *gfx_row(gfx_buffer_t *buf, int32_t row) {
    return (gfx_color_t *)((uint8_t *)(buf->buffer) + (row * buf->pitch));
}

void gfx_clear(gfx_buffer_t *buf, gfx_color_t color) {
    for (int32_t r = 0; r < buf->height; r++) {
        gfx_color_t *row = gfx_row(buf, r);
        for (int32_t c = 0; c < buf->width; c++) {
            row[c] = color;
        }
    }
}

void gfx_fill_rect(gfx_buffer_t *buf, int32_t x, int32_t y, int32_t w, int32_t h, gfx_color_t color) {
    if (gfx_color_is_clear(color)) {
        return; 
    }

    if (!intervals_overlap(&x, &w, buf->width)) {
        return;
    }

    if (!intervals_overlap(&y, &h, buf->height)) {
        return;
    }

    for (int32_t r = y; r < y + h; r++) {
        gfx_color_t *row = gfx_row(buf, r);
        for (int32_t c = x; c < x + w; c++) {
            row[c] = color;
        }
    }
}

void gfx_fill_bitmap(gfx_buffer_t *buf, 
        int32_t x, int32_t y, uint8_t w_scale, uint8_t h_scale,
        uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, 
        gfx_color_t fg_color, gfx_color_t bg_color) {

    const bool fg_is_clear = gfx_color_is_clear(fg_color);
    const bool bg_is_clear = gfx_color_is_clear(bg_color);

    // Both colors are clear, don't even bother.
    if (fg_is_clear && bg_is_clear) {
        return;
    }

    /**
     * Size of each bitmap row in BYTES!!!
     */
    const int32_t bitmap_row_size = bitmap_cols % 8 == 0
        ? bitmap_cols / 8 : (bitmap_cols / 8) + 1;

    // Ok, now, let's think about much area this bitmap will occupy.
    //
    // All of the values below are int32's to avoid arithmetic warnings.
    // However, after the `intervals_overlap` calls, they'll all be gauranteed to be
    // positive!
    //
    // This is all safe also because both `x` and `y` must be greater than INT32_MIN in the
    // case of overlap!

    int32_t start_x = x;
    int32_t width = w_scale * bitmap_cols; // Impossible to wrap as 0xFF * 0xFF < 0x1_0000UL < INT32_MAX
    if (!intervals_overlap(&start_x, &width, buf->width)) {
        return;
    }
    int32_t end_x = start_x + width - 1; // Inclusive

    int32_t start_y = y;
    int32_t height = h_scale * bitmap_rows;
    if (!intervals_overlap(&start_y, &height, buf->height)) {
        return;
    }
    int32_t end_y = start_y + height - 1; // Inclusive

    for (int32_t y_iter = start_y; y_iter <= end_y; y_iter++) {
        gfx_color_t *row = gfx_row(buf, y_iter);
        int32_t row_ind = (y_iter - y) / h_scale;

        for (int32_t x_iter = start_x; x_iter <= end_x; x_iter++) {
            int32_t col_ind = (x_iter - x) / w_scale;

            bool fg_pixel = bitmap[(row_ind * bitmap_row_size) + (col_ind / 8)] & 
                (1 << (7 - (col_ind % 8))); // Remember, left to right!

            if (fg_pixel && !fg_is_clear) {
                row[x_iter] = fg_color;
            } else if (!fg_pixel && !bg_is_clear) {
                row[x_iter] = bg_color;
            }
        }
    }
}
