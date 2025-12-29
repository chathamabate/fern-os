
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

/**
 * Here we have two interval on a number line.
 *
 * `[*pos, *pos + len)` and `[0, window_len)`.
 * 
 * `true` is returned iff the two intervals overlap.
 *
 * If `true` is returned, the overlapping position is written to `*pos` and `*len`.
 *
 * If `*len` or `window_len` are negative or 0, `false` is always returned.
 */
static bool intervals_overlap(int32_t *pos, int32_t *len, int32_t window_len)  {
    int32_t p = *pos;  
    int32_t l = *len;

    if (l <= 0 || window_len <= 0) {
        return false;
    }

    if (p < 0) {
        // With p < 0, wrap is impossible here given `l` is positive.
        if (p + l <= 0) {
            return false;
        }
        
        // Chop off left side. `l` must be greater than zero after the chop.
        l += p;
        p = 0;
    } else if (p >= window_len) {
        return false; // Too far off to the right.
    } else { /* 0 <= p && p < window_len */
        // Since `p` is positive, now there is a chance for wrap when adding
        // another positive number.
        if (p + l < 0) { 
            // This makes the maximum possible value of `p + l` INT32_MAX.
            // WHICH IS OK! because the largest window interval describeable is
            // [0, INT32_MAX)... INT32_MAX will never be able to be in the overlapping
            // interval.
            l = INT32_MAX - p; 
        }

        if (p + l > window_len) {
            // Chop off right hanging side.
            l = window_len - p;
        }
    }

    *pos = p;
    *len = l;

    return true;
}

void gfxv_fill_rect(gfx_view_t *view, 
        int32_t x, int32_t y, int32_t w, int32_t h, gfx_color_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        if (x + w <= 0) {
            return; // Fully off screen to the left.
        }

        // Chop off area to the left of screen.
        w += x;
        x = 0;
    } else if (/* x >= 0 && */ x < view->width) {
        if (x + w > view->width) {
            w = view->width - x;
        }
    } else /* if (x >= view->width) */ {
        return;
    }
    // Does this box even intersect with the view??
    // Does the view even intersect with the parent buffer?
}
