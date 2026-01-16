
#include "s_gfx/window_qgrid.h"

#if WIN_QGRID_BORDER_WIDTH < WIN_QGRID_FOCUS_BORDER_WIDTH
#error "Focused border width must be <= border width!"
#endif

void delete_window_qgrid(window_t *w);
fernos_error_t win_qg_on_event(window_t *w, window_event_t ev);
fernos_error_t win_qg_register_child(window_t *w, window_t *sw);
void win_qg_deregister_child(window_t *w, window_t *sw);

static const window_impl_t QGRID_IMPL = {
    .delete_window = delete_window_qgrid,
    .win_on_event = win_qg_on_event,
    .win_register_child = win_qg_register_child,
    .win_deregister_child = win_qg_deregister_child
};

window_t *new_window_qgrid(allocator_t *al, uint16_t width, uint16_t height) {
    if (!al) {
        return NULL;
    }

    if (width < WIN_QGRID_MIN_DIM || height < WIN_QGRID_MIN_DIM) {
        return NULL;
    }
    
    window_qgrid_t *win_qg = al_malloc(al, sizeof(window_qgrid_t));
    gfx_buffer_t *buf = new_gfx_buffer(al, width, height);

    if (!win_qg || !buf) {
        delete_gfx_buffer(buf);
        al_free(al, win_qg);

        return NULL;
    }

    // This can be defined on the stack as it will be deeply copied into the
    // window structure!
    window_attrs_t qg_attrs = {
        .min_width = WIN_QGRID_MIN_DIM,
        .max_width = UINT16_MAX,

        .min_height = WIN_QGRID_MIN_DIM,
        .max_height = UINT16_MAX,
    };

    init_window_base((window_t *)win_qg, buf, &qg_attrs,
            &QGRID_IMPL);
    *(allocator_t **)&(win_qg->al) = al;

    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            win_qg->grid[r][c] = NULL;
        }
    }

    win_qg->focused_row = 0;
    win_qg->focused_col = 0;

    win_qg->single_pane_mode = false;
    win_qg->cntl_held = false;
    win_qg->focused = false;

    return (window_t *)win_qg;
}

void delete_window_qgrid(window_t *w) {
    window_qgrid_t *win_qg = (window_qgrid_t *)w;

    // First last deregister all children!
    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            // Works just fine event if (r, c) holds NULL already.
            win_deregister(win_qg->grid[r][c]);
            win_qg->grid[r][c] = NULL;
        }
    }

    // Now delete self.
    deinit_window_base(w);
    al_free(win_qg->al, win_qg);
}

/**
 * This is unsafe and assumes that x, y, w, and h form a box entirely visible on 
 * `win_qg->buf`.
 *
 * It also assumes there is room for the focus border!
 */
static void win_qg_render_tile(window_qgrid_t *win_qg, window_t *sw, uint16_t x, uint16_t y, 
        uint16_t w, uint16_t h, bool focused) {
    gfx_box_t clip = {
        .x = x, .y = y, .width = w, .height = h
    };
    gfx_paste_buffer(win_qg->super.buf, &clip, sw->buf, x, y);

    const gfx_color_t gap_fill_color = gfx_color(0, 0, 50);

    // I kinda go heavily out the way to not need to paint twice over the same pixel.

    if (sw->buf->width < w) {
        gfx_fill_rect(win_qg->super.buf, NULL,
            x + sw->buf->width, y, 
            w - sw->buf->width, sw->buf->height, 
            gap_fill_color
        ); 
    }

    if (sw->buf->height < h) {
        gfx_fill_rect(win_qg->super.buf, NULL,
            x, y + sw->buf->height,
            sw->buf->width, h - sw->buf->height,
            gap_fill_color
        );
    }

    if (sw->buf->width < w && sw->buf->height < h) {
        gfx_fill_rect(win_qg->super.buf, NULL,
            x + sw->buf->width, y + sw->buf->height,
            w - sw->buf->width, h - sw->buf->height, 
            gap_fill_color
        );
    }

    const gfx_color_t focus_border_color = gfx_color(255, 255, 255);

    if (focused) {
        gfx_draw_rect(
           win_qg->super.buf, NULL,
           x - WIN_QGRID_FOCUS_BORDER_WIDTH,
           y - WIN_QGRID_FOCUS_BORDER_WIDTH, 
           w + (2 * WIN_QGRID_FOCUS_BORDER_WIDTH),
           h + (2 * WIN_QGRID_FOCUS_BORDER_WIDTH),
           WIN_QGRID_FOCUS_BORDER_WIDTH, 
           focus_border_color
        );
    }
}

static void win_qg_render(window_qgrid_t *win_qg) {
    gfx_buffer_t * const buf = win_qg->super.buf;

    const uint16_t tile_width = (buf->width - WIN_QGRID_BORDER_WIDTH) / 2;
    const uint16_t tile_height = (buf->height - WIN_QGRID_BORDER_WIDTH) / 2;

    const gfx_color_t border_color = gfx_color(150, 50, 50);
    const gfx_color_t focused_border_color = gfx_color(200, 50, 50);
    const gfx_color_t empty_tile_color = gfx_color(120, 50, 120);

    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            window_t *sw = win_qg->grid[r][c];

        }
    }
}

fernos_error_t win_qg_on_event(window_t *w, window_event_t ev) {
    window_qgrid_t *win_qg = (window_qgrid_t *)w;

    switch (ev.event_code) {

    case WINEC_TICK: {
        // Here we actually render to buffer.
        return FOS_E_NOT_IMPLEMENTED;         
    }

    case WINEC_RESIZED: {
        // Here we resize children.
        return FOS_E_NOT_IMPLEMENTED;            
    }

    case WINEC_KEY_INPUT: {
        return FOS_E_NOT_IMPLEMENTED;            
    }

    case WINEC_DEREGISTERED: {
        return FOS_E_SUCCESS;
    }

    case WINEC_FOCUSED: {
        win_qg->focused = true;
        return FOS_E_SUCCESS;
    }

    case WINEC_UNFOCUSED: {
        win_qg->focused = false;
        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_SUCCESS;
    }

    }
}

fernos_error_t win_qg_register_child(window_t *w, window_t *sw) {
    return FOS_E_NOT_IMPLEMENTED;
}

void win_qg_deregister_child(window_t *w, window_t *sw) {

}


