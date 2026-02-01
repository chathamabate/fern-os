
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
    win_d->tick_num = 0;

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
        gfx_color(75, 0, 0),
        gfx_color(25, 0, 25)
    }; 

    const gfx_color_t focused_cursor_color = gfx_color(150, 0, 0);
    const gfx_color_t unfocused_cursor_color = gfx_color(50, 0, 50);

    int32_t y = grid_y;
    for (size_t r = 0; r < WIN_DUMMY_ROWS; r++, y += WIN_DUMMY_CELL_DIM) {
        int32_t x = grid_x;
        for (size_t c = 0; c < WIN_DUMMY_COLS; c++, x += WIN_DUMMY_CELL_DIM) {
            gfx_color_t tile_color = tile_colors[(r + c) & 1];
            if (r == win_d->cursor_row && c == win_d->cursor_col) {
                tile_color = win_d->focused ? focused_cursor_color : unfocused_cursor_color;
            }

            gfx_fill_rect(w->buf, NULL, x, y, 
                    WIN_DUMMY_CELL_DIM, WIN_DUMMY_CELL_DIM, tile_color
            );
        }
    }
}

static fernos_error_t win_d_on_event(window_t *w, window_event_t ev) {
    window_dummy_t *win_d = (window_dummy_t *)w;

    switch (ev.event_code) {

    case WINEC_KEY_INPUT: {
        switch (ev.d.key_code) {

        case SCS1_E_UP: {
            if (win_d->cursor_row > 0) {
                win_d->cursor_row--;
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_DOWN: {
            if (win_d->cursor_row < WIN_DUMMY_ROWS - 1) {
                win_d->cursor_row++;
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_LEFT: {
            if (win_d->cursor_col > 0) {
                win_d->cursor_col--;
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_RIGHT: {
            if (win_d->cursor_col < WIN_DUMMY_COLS - 1) {
                win_d->cursor_col++;
            }
            return FOS_E_SUCCESS;
        }

        default: {
            return FOS_E_SUCCESS;
        }

        }
    }

    case WINEC_TICK: {
        win_d->tick_num++;
        return FOS_E_SUCCESS;
    }

    case WINEC_FOCUSED: {
        win_d->focused = true;
        return FOS_E_SUCCESS;
    }

    case WINEC_UNFOCUSED: {
        win_d->focused = false;
        return FOS_E_SUCCESS;
    }

    case WINEC_DEREGISTERED: {
        // We are self managed!
        // Delete here!
        delete_window_dummy(w);
        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_SUCCESS;
    }

    }
}
