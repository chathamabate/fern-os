

#include "s_gfx/test/gfx.h"

void gfx_test_bouncing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    const uint16_t rect_w = 100;
    const uint16_t rect_h = 150;

    static int32_t x_pos = 0;
    static int32_t x_per_frame = 4;
    static int32_t y_pos = 0;
    static int32_t y_per_frame = 3;

    x_pos += x_per_frame;
    if (x_pos + rect_w >= buffer->width) {
        x_pos = buffer->width - rect_w;
        x_per_frame *= -1;
    } else if (x_pos < 0) {
        x_pos = 0;
        x_per_frame *= -1;
    }

    y_pos += y_per_frame;
    if (y_pos + rect_h >= buffer->height) {
        y_pos = buffer->height - rect_h;
        y_per_frame = -1;
    } else if (y_pos < 0) {
        y_pos = 0;
        y_per_frame  = 1;
    }
    
    gfx_fill_rect(buffer, clip, x_pos, y_pos, 
            rect_w, rect_h, gfx_color(100, 0, 200));
}

void gfx_test_outside_bouncing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    const uint16_t rect_w = 250;
    const uint16_t rect_h = 200;

    static int32_t x_pos = 0;
    static int32_t x_per_frame = 4;
    static int32_t y_pos = 0;
    static int32_t y_per_frame = 6;

    gfx_box_t bounds = {
        .x = -100, .y = -100,
        .width = buffer->width + 200,
        .height = buffer->height + 400
    };

    x_pos += x_per_frame;
    if (x_pos + rect_w >= bounds.x + bounds.width) {
        x_pos = bounds.x + bounds.width - rect_w;
        x_per_frame *= -1;
    } else if (x_pos < bounds.x) {
        x_pos = bounds.x;
        x_per_frame *= -1;
    }

    y_pos += y_per_frame;
    if (y_pos + rect_h >= bounds.y + bounds.height) {
        y_pos = bounds.y + bounds.height - rect_h;
        y_per_frame *= -1;
    } else if (y_pos < bounds.y) {
        y_pos = bounds.y;
        y_per_frame *= -1;
    }

    gfx_fill_rect(buffer, clip, x_pos, y_pos, 
            rect_w, rect_h, gfx_color(200, 0, 200));
}
