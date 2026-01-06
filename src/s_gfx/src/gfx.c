
#include "s_gfx/gfx.h"
#include <limits.h>
#include "s_util/str.h"
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
    if (clip_area->width == 0 || clip_area->height == 0 ||
            box->width == 0 || box->height == 0) {
        return false; // Can't intersect with an empty area!
    }

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
        box->width -= clip_area->x - box->x;
        box->x = clip_area->x;
    }

    if (clip_x_bound < box_x_bound) {
        box->width -= box_x_bound - clip_x_bound;
    }

    if (box->y < clip_area->y) {
        box->height -= clip_area->y - box->y;
        box->y = clip_area->y;
    }

    if (clip_y_bound < box_y_bound) {
        box->height -= box_y_bound - clip_y_bound;
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

void gfx_fill_box(gfx_buffer_t *buf, const gfx_box_t *clip_area, 
        gfx_box_t box, gfx_color_t color) {
    if (gfx_color_is_clear(color)) {
        return; 
    }

    if (!gfx_clip_with_buffer(buf, clip_area, &box)) {
        return;
    }

    gfx_color_t *row0 = buf->buffer + (box.y * buf->width);
    for (int32_t x_i = box.x; x_i < box.x + box.width; x_i++) {
        row0[x_i] = color;
    }

    gfx_color_t *row = row0 + buf->width;
    for (int32_t y_i = box.y + 1; y_i < box.y + box.height; y_i++, row += buf->width) {
        mem_cpy(row + box.x, row0 + box.x, box.width * sizeof(gfx_color_t));
    }
}

void gfx_fill_bitmap(gfx_buffer_t *buf, const gfx_box_t *clip_area,
        int32_t x, int32_t y, 
        uint8_t w_scale, uint8_t h_scale,
        const uint8_t *bitmap, uint8_t bitmap_rows, uint8_t bitmap_cols, 
        gfx_color_t fg_color, gfx_color_t bg_color) {

    // Both colors are clear, don't even bother.
    if (gfx_color_is_clear(fg_color) && gfx_color_is_clear(bg_color)) {
        return;
    }

    gfx_box_t render_area = {
        .x = x, .y = y,
        .width = (uint16_t)w_scale * bitmap_cols,
        .height = (uint16_t)h_scale * bitmap_rows
    };

    if (!gfx_clip_with_buffer(buf, clip_area, &render_area)) {
        return;
    }

    const int32_t render_area_end_x = render_area.x + render_area.width - 1;
    const int32_t render_area_end_y = render_area.y + render_area.height - 1;

    // At this point, bitmap_cols > 0 always.
    const uint32_t bitmap_row_size = ((bitmap_cols - 1) / 8) + 1;

    const int32_t start_row = (render_area.y - y) / h_scale;
    const int32_t end_row = (render_area_end_y - y) / h_scale; // Inclusive!

    const int32_t start_col = (render_area.x - x) / w_scale;
    const int32_t end_col = (render_area_end_x - x) / w_scale; // Inclusive!

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
            if (rect_start_x < render_area.x) {
                rect_start_x = render_area.x;
            }
            if (rect_end_x > render_area_end_x) {
                rect_end_x = render_area_end_x;
            }

            int32_t rect_start_y = y + (row_ind * h_scale);
            int32_t rect_end_y = rect_start_y + h_scale - 1;
            if (rect_start_y < render_area_end_y) {
                rect_start_y = render_area.y;
            }
            if (rect_end_y > render_area_end_y) {
                rect_end_y = render_area_end_y;
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
