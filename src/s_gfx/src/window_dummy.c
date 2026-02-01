
#include "s_gfx/window_dummy.h"
#include <stdatomic.h>

static void delete_window_dummy(window_t *w);
static void win_d_render(window_t *w);
static fernos_error_t win_d_on_event(window_t *w, window_event_t ev);

static const window_impl_t D_IMPL = {
    .delete_window = delete_window_dummy,
    .win_render = win_d_render,
    .win_on_event = win_d_on_event,
    .win_register_child = NULL,
    .win_deregister_child = NULL 
};

window_t *new_window_dummy(allocator_t *al) {
    if (!al) {
        return NULL;
    }

    window_dummy_t *win_d = al_malloc(al, sizeof(window_dummy_t));
    gfx_buffer_t *buf = new_gfx_buffer(al, WIN_DUMMY_GRID_WIDTH, WIN_DUMMY_GRID_HEIGHT);

    if (!win_d || !buf) {
        delete_gfx_buffer(buf);
        al_free(al, win_d);

        return NULL;
    }

    // SUCCESS!

    window_attrs_t d_attrs = {
        .min_width = WIN_DUMMY_GRID_WIDTH,
        .max_width = UINT16_MAX,

        .min_height = WIN_DUMMY_GRID_HEIGHT,
        .max_height = UINT16_MAX,
    };

    init_window_base((window_t *)win_d, buf, &d_attrs, &D_IMPL);

    *(allocator_t **)&(win_d->al) = al;

    win_d->cursor_row = WIN_DUMMY_ROWS / 2;
    win_d->cursor_col = WIN_DUMMY_COLS / 2;

    win_d->focused = false;

    return (window_t *)win_d;
}

static void delete_window_dummy(window_t *w) {
    window_dummy_t *win_d = (window_dummy_t *)w;

    deinit_window_base(w);
    al_free(win_d->al, win_d);
}

static void win_d_render(window_t *w) {
    const uint16_t grid_x = (w->buf->width - WIN_DUMMY_GRID_WIDTH) / 2;
    const uint16_t grid_y = (w->buf->height - WIN_DUMMY_GRID_HEIGHT) / 2;

    window_dummy_t *win_d = (window_dummy_t *)w;

    const gfx_color_t bg_color = gfx_color(0, 0, 0);

    gfx_clear(w->buf, bg_color);

    const gfx_color_t tile_colors[2] = {
        gfx_color(100, 0, 0),
        gfx_color(0, 0, 100)
    }; 

    const gfx_color_t cursor_color = gfx_color(140, 140, 140);
    const gfx_color_t focused_cursor_color = gfx_color(200, 0, 0);

    int32_t y = grid_y;
    for (size_t r = 0; r < WIN_DUMMY_ROWS; r++, y += WIN_DUMMY_CELL_DIM) {
        int32_t x = grid_x;
        for (size_t c = 0; c < WIN_DUMMY_COLS; c++, x += WIN_DUMMY_CELL_DIM) {
            gfx_color_t tile_color = tile_colors[(r + c) & 1];
            if (r == win_d->cursor_row && c == win_d->cursor_col) {
                tile_color = win_d->focused ? focused_cursor_color : cursor_color;
            }

            gfx_fill_rect(w->buf, NULL, x, y, 
                    WIN_DUMMY_CELL_DIM, WIN_DUMMY_CELL_DIM, tile_color
            );
        }
    }
}

static fernos_error_t win_d_on_event(window_t *w, window_event_t ev) {
    return FOS_E_SUCCESS;
}
