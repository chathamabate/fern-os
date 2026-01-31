
#include "s_gfx/window_dummy.h"

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
    uint16_t grid_x = (w->buf->width - WIN_DUMMY_GRID_WIDTH) / 2;
    uint16_t grid_y = (w->buf->height - WIN_DUMMY_GRID_HEIGHT) / 2;

    const gfx_color_t bg_color = gfx_color(0, 0, 0);

    const gfx_color_t grid_color = gfx_color(100, 100, 100);
    const gfx_color_t cursor_color = gfx_color(140, 140, 140);

    const gfx_color_t focused_grid_color = gfx_color(130, 130, 130);
    const gfx_color_t focused_cursor_color = gfx_color(200, 0, 0);

    gfx_clear(w->buf, gfx_color(100, 0, 0));

    // Honestly, more to think about than you'd expect here..
    // Maybe I should take a break/go to bed, after all, my first day is tomorrow!

}

static fernos_error_t win_d_on_event(window_t *w, window_event_t ev) {
    return FOS_E_SUCCESS;
}
