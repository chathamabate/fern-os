
#include "s_gfx/gfx.h"
#include <limits.h>
#include "s_util/misc.h"

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
