

#include "k_startup/vga_cd.h"
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

#include "s_gfx/test/gfx.h"

/**
 * Test clipping boxes which don't overlap.
 */
static bool test_gfx_clip_misses(void) {
    const gfx_box_t cases[][2] = {
        {
            {0, 0, 10, 10},

            // Too far to the left.
            {-10, 0, 5, 10}
        },
        {
            {20, 0, 20, 20},

            // Too far to the right.
            {80, 0, 20, 20}
        },
        {
            {0, 0, 10, 10},

            // Too far above.
            {0, INT32_MIN, 0, 1000}
        },
        {
            {100, -100, 40, 50},

            // Too far Below!
            {0, -50, 100, 100}
        }
    };
    const size_t num_cases = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < num_cases; i++) {
        const gfx_box_t clip_area = cases[i][0];
        gfx_box_t other = cases[i][1];

        TEST_FALSE(gfx_clip(&clip_area, &other));

        TEST_EQUAL_INT(cases[i][1].x, other.x);
        TEST_EQUAL_INT(cases[i][1].y, other.y);
        TEST_EQUAL_UINT(cases[i][1].width, other.width);
        TEST_EQUAL_UINT(cases[i][1].height, other.height);
    }

    TEST_SUCCEED();
}

bool test_gfx(void) {
    BEGIN_SUITE("GFX Logic");
    RUN_TEST(test_gfx_clip_misses);
    return END_SUITE();
}

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

void gfx_test_bitmaps(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    const struct bm_case_t {
        uint8_t cols;
        uint8_t rows;
        uint8_t map[16];
    } bitmaps[] = {
        // First bitmap will be a 5x4 Box
        {
            .cols = 5, .rows = 4,
            .map = {
                0xF8,
                0x90,
                0x90,
                0xF8
            }
        },

        // Second Bitmap will be a 10x2 dotted line.
        {
            .cols = 10, .rows = 2,
            .map = {
                0xAA, 0x80,
                0xAA, 0x80
            }
        },

        // Third will be a 8x8 Letter 'F'.
        {
            .cols = 8, .rows = 8,
            .map = {
                0xFF,
                0xFE,
                0xE0,
                0xE0,
                0xFE,
                0xFF,
                0xE0,
                0xC0
            }
        }
    };

    const size_t num_bms = sizeof(bitmaps) / sizeof(bitmaps[0]);

    uint16_t y = 100;

    for (size_t i = 0; i < num_bms; i++) {
        // For each bitmap, we'll draw three scaled versions.
        struct bm_case_t c = bitmaps[i];

        for (size_t w_scale_addon = 0; w_scale_addon <= 2; w_scale_addon++) {
            uint16_t x = 100;
            for (size_t scale = 1; scale <= 7; scale++) {
                gfx_fill_bitmap(buffer, clip, x, y, 
                        scale + w_scale_addon, scale, c.map, c.rows, c.cols,
                        gfx_color(0, 255, 0),
                        gfx_color(0, 0, 0));

                x += ((scale + w_scale_addon) * c.cols) + 20;
            }

            y += 70;
        }
    }
}

void gfx_test_outside_bouncing_bitmap(gfx_buffer_t *buffer, const gfx_box_t *clip) {
    // This should be an upward arrow with a tail to the left. 10 x 6
    const uint8_t bitmap[] = {
        0x0C, 0x00, // 0000 1100 0000
        0x1E, 0x00, // 0001 1110 0000
        0x3F, 0x00, // 0011 1111 0000
        0x6D, 0x80, // 0110 1101 1000
        0xCC, 0xC0, // 1100 1100 1100
        0x0C, 0x00, // 0000 1100 0000
        0x0C, 0x00, // 0000 1100 0000
        0xFC, 0x00, // 1111 1100 0000
    };
    const uint8_t rows = 8;
    const uint8_t cols = 10;

    const uint8_t w_scale = 10;
    const uint8_t h_scale = 10;

    const uint16_t width = (uint16_t)cols * w_scale;
    const uint16_t height = (uint16_t)rows * h_scale;

    static int32_t x_pos = 100;
    static int32_t x_per_frame = 4;

    static int32_t y_pos = 100;
    static int32_t y_per_frame = 7;

    const uint16_t pad = 100;

    x_pos += x_per_frame;
    if (x_pos + width >= buffer->width + pad) {
        x_pos = (int32_t)(buffer->width + pad) - width;
        x_per_frame *= -1;
    } else if (x_pos <= -(int32_t)pad) {
        x_pos = -(int32_t)pad;
        x_per_frame *= -1;
    }

    y_pos += y_per_frame;
    if (y_pos + height >= buffer->height + pad) {
        y_pos = (int32_t)(buffer->height + pad) - height;
        y_per_frame *= -1;
    } else if (y_pos <= -(int32_t)pad) {
        y_pos = -(int32_t)pad;
        y_per_frame *= -1;
    }

    gfx_fill_bitmap(buffer, clip, x_pos, y_pos, w_scale, h_scale, bitmap, rows, cols, 
            gfx_color(255, 0, 0), GFX_COLOR_CLEAR);
}
