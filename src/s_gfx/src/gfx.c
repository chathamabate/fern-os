
#include "s_gfx/gfx.h"
#include <limits.h>

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

gfx_view_t *new_gfx_view(allocator_t *al, gfx_buffer_t *pb, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (!al || !pb || w < 0 || h < 0) {
        return NULL;
    }

    gfx_view_t *view = al_malloc(al, sizeof(gfx_view_t));
    if (!view) {
        return NULL;
    }

    view->al = al;
    view->parent_buffer = pb;
    view->origin_x = x;
    view->origin_y = y;
    view->width = w;
    view->height = h;

    return view;
}

void delete_gfx_view(gfx_view_t *view) {
    if (view) {
        al_free(view->al, view);
    }
}

void gfxv_fill_rect(gfx_view_t *view, int32_t x, int32_t y, int32_t w, int32_t h, gfx_color_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        if (x == INT32_MIN) {
            return;
        }

        if (x + w < ) {

        }
        
        x = 0;

    }
    // Does this box even intersect with the view??
    // Does the view even intersect with the parent buffer?
}
