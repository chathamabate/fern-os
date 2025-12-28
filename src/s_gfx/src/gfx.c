
#include "s_gfx/gfx.h"

/**
 * Return a pointer to row `row` within `buf`.
 *
 * THIS DOES NO CHECKS ON `row` !
 */
static gfx_color_t *gfx_row(gfx_buffer_t *buf, uint16_t row) {
    return (gfx_color_t *)((uint8_t *)(buf->buffer) + (row * buf->pitch));
}

void gfx_clear(gfx_buffer_t *buf, gfx_color_t color) {
    for (uint16_t r = 0; r < buf->height; r++) {
        gfx_color_t *row = gfx_row(buf, r);
        for (uint16_t c = 0; c < buf->width; c++) {
            row[c] = color;
        }
    }
}
