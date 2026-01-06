

#include "s_gfx/test/gfx.h"

void gfx_test_rect_grid(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    const uint16_t rect_w = 100;
    const uint16_t rect_h = 50;

    for (uint16_t x = 0; x < buffer->width; x += rect_w) {
        for (uint16_t y = 0; y < buffer->height; y += rect_h) {
            uint16_t width = rect_w - 1; 
            if (x + width > buffer->width) {
                width = buffer->width - x;
            }

            uint16_t height = rect_h - 1;
            if (y + height > buffer->height) {
                height = buffer->height - y;
            }

            gfx_fill_rect(buffer, clip, x, y, width, height, 
                    gfx_color((x / rect_w) * 15, 0, (y / rect_h) * 15));
        }
    }
}

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

void gfx_test_growing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    static int32_t width = 10;
    static int32_t width_per_frame = 5;

    const int32_t width_max = 1300;

    static int32_t height = 10;
    static int32_t height_per_frame = 4;

    const int32_t height_max = 1200;

    width += width_per_frame;
    if (width >= width_max) {
        width = width_max;
        width_per_frame *= -1;
    } else if (width <= 0) {
        width = 0;
        width_per_frame *= -1;
    }

    height += height_per_frame;
    if (height >= height_max) {
        height = height_max;
        height_per_frame *= -1;
    } else if (height <= 0) {
        height = 0;
        height_per_frame *= -1;
    }

    gfx_fill_rect(buffer, clip, 
            (buffer->width - width) / 2, 
            (buffer->height - height) / 2,
            width, height, gfx_color(200, 100, 0));
}

