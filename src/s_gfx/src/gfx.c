
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

void gfx_clear(gfx_buffer_t *buf, gfx_color_t color) {
    for (uint16_t r = 0; r < buf->height; r++) {
        gfx_color_t *row = buf->buffer + (r * buf->width);
        for (uint16_t c = 0; c < buf->width; c++) {
            row[c] = color;
        }
    }
}

bool gfx_clip(const gfx_box_t *clip_area, gfx_box_t *box) {
    int32_t clip_x_bound = clip_area->x + clip_area->width;
    if (clip_x_bound <= box->x) {
        return false;
    }
    int32_t box_x_bound = box->x + box->width;
    if (box_x_bound <= clip_area->x) {
        return false;
    }

    int32_t clip_y_bound = clip_area->y + clip_area->height;
    if (clip_y_bound <= box->y) {
        return false;
    }

    int32_t box_y_bound = box->y + box->height;
    if (box_y_bound <= clip_area->y) {
        return false;
    }

    // Intersection!
    
    if (box->x < clip_area->x) {
        box->x = clip_area->x;
    }

    if (clip_x_bound < box_x_bound) {
        box->width = clip_x_bound - box->x;
    }

    if (box->y < clip_area->y) {
        box->y = clip_area->y;
    }

    if (clip_y_bound < box_y_bound) {
        box->height = clip_y_bound - box->y;
    }

    return true;
}

bool gfx_clip_with_buffer(const gfx_buffer_t *buf, 
        const gfx_box_t *clip_area, gfx_box_t *box) {
    gfx_box_t buffer_box = {
        .x = 0, .y = 0,
        .width = buf->width, .height = buf->height
    };

    if (clip_area) {
        if (!gfx_clip(clip_area, &buffer_box)) {
            return false;
        }
    }

    return gfx_clip(&buffer_box, box);
}

void gfx_fill_rect(gfx_buffer_t *buf, const gfx_box_t *clip_area, 
        int32_t x, int32_t y, uint16_t w, uint16_t h, gfx_color_t color) {
    if (gfx_color_is_clear(color)) {
        return; 
    }

    gfx_box_t rect = {
        .x = x, .y = y,
        .width = w, .height = h
    };
    
    if (!gfx_clip_with_buffer(buf, clip_area, &rect)) {
        return;
    }

    for (int32_t y_i = rect.y; y_i < rect.y + rect.height; y_i++) {
        gfx_color_t *row = buf->buffer + (y_i * buf->width);
        for (int32_t x_i = rect.x; x_i < rect.x + rect.width; x_i++) {
            row[x_i] = color;
        }
    }
}

void gfxv_fill_bitmap(gfx_view_t *view,
        int32_t x, int32_t y, 
        uint8_t w_scale, uint8_t h_scale,
        const uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, 
        gfx_color_t fg_color, gfx_color_t bg_color) {

    // Both colors are clear, don't even bother.
    if (gfx_color_is_clear(fg_color) && gfx_color_is_clear(bg_color)) {
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
    
    // Ok, start with relative to the view.
    
    int32_t start_x = x;
    int32_t width = w_scale * bitmap_cols; // Impossible to wrap as 0xFF * 0xFF < 0x1_0000UL < INT32_MAX
    if (!intervals_overlap(&start_x, &width, view->width)) {
        return;
    }

    int32_t start_y = y;
    int32_t height = h_scale * bitmap_rows;
    if (!intervals_overlap(&start_y, &height, view->height)) {
        return;
    }

    // Now, move to relative to the buffer.
    start_x += view->x; // Gauranteed to not wrap!
    start_y += view->y;

    if (!intervals_overlap(&start_x, &width, view->buffer->width)) {
        return;
    }
    if (!intervals_overlap(&start_y, &height, view->buffer->height)) {
        return;
    }

    // Might this wrap?? Isn't this all very confusing??
    // Why do we check so hard for wrap, isn't that hella slow (AND CONFUSING??)
    const int32_t abs_x = x + view->x;
    const int32_t abs_y = y + view->y;

    int32_t end_x = start_x + width - 1; // Inclusive
    int32_t start_col = (start_x - x) / w_scale;
    int32_t end_col = (end_x - x) / w_scale;

    int32_t end_y = start_y + height - 1; // Inclusive
    int32_t start_row = (start_y - y) / h_scale;
    int32_t end_row = (end_y - y) / h_scale;

    for (int32_t row_ind = start_row; row_ind <= end_row; row_ind++) {
        for (int32_t col_ind = start_col; col_ind <= end_col; col_ind++) {
            // Each pixel in the bitmap will become a `h_scale X w_scale` rectangle 
            // in the buffer!

            bool fg_pixel = bitmap[(row_ind * bitmap_row_size) + (col_ind / 8)] & 
                (1 << (7 - (col_ind % 8))); // Remember, left to right!
                                            
            // In the case where this pixel is gauranteed to be transparent,
            // skip all the work below.

            gfx_color_t rect_color = fg_pixel ? fg_color : bg_color;
            if (gfx_color_is_clear(rect_color)) {
                continue;
            }

            int32_t rect_start_x = x + (col_ind * w_scale);
            int32_t rect_end_x = rect_start_x + w_scale - 1;
            if (rect_start_x < 0) {
                rect_start_x = 0;
            }
            if (rect_end_x >= buf->width) {
                rect_end_x = buf->width - 1;
            }

            int32_t rect_start_y = y + (row_ind * h_scale);
            int32_t rect_end_y = rect_start_y + h_scale - 1;
            if (rect_start_y < 0) {
                rect_start_y = 0;
            }
            if (rect_end_y >= buf->height) {
                rect_end_y = buf->height - 1;
            }
            
            // Fill the rectangle!
            for (int32_t y_iter = rect_start_y; y_iter <= rect_end_y; y_iter++) {
                gfx_color_t *buf_row = buf->buffer + (y_iter * buf->width);
                for (int32_t x_iter = rect_start_x; x_iter <= rect_end_x; x_iter++) {
                    buf_row[x_iter] = rect_color;
                }
            }
        }
    }
}
