
#include "s_gfx/window_dummy.h"
#include "s_data/term_buffer.h"
#include "s_gfx/mono_fonts.h"
#include "s_util/ansi.h"

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

    const uint16_t init_rows = 20;
    const uint16_t init_cols = 50;

    window_dummy_t *win_d = al_malloc(al, sizeof(window_dummy_t));
    gfx_buffer_t *buf = new_gfx_buffer(al, init_cols * WINDOW_DUMMY_FONT->char_width, init_rows * WINDOW_DUMMY_FONT->char_height);
    term_buffer_t *vtb = new_term_buffer(al, 
            (term_cell_t) {.c = ' ', .style = term_style(TC_WHITE, TC_BLACK)},
            init_rows, init_cols);
    term_buffer_t *rtb = new_term_buffer(al, 
            (term_cell_t) {.c = ' ', .style = term_style(TC_WHITE, TC_BLACK)},
            init_rows, init_cols);

    if (!win_d || !buf || !vtb || !rtb) {
        delete_term_buffer(rtb);
        delete_term_buffer(vtb);
        delete_gfx_buffer(buf);
        al_free(al, win_d);

        return NULL;
    }

    // SUCCESS!

    window_attrs_t d_attrs = {
        .min_width = 0,
        .max_width = UINT16_MAX,

        .min_height = 0,
        .max_height = UINT16_MAX,
    };

    init_window_base((window_t *)win_d, buf, &d_attrs, &D_IMPL);

    *(allocator_t **)&(win_d->al) = al;
    *(term_buffer_t **)&(win_d->visible_tb) = vtb;
    win_d->dirty_buffer = true;
    *(term_buffer_t **)&(win_d->real_tb) = rtb;

    return (window_t *)win_d;
}

static void delete_window_dummy(window_t *w) {
    window_dummy_t *win_d = (window_dummy_t *)w;

    delete_term_buffer(win_d->real_tb);
    delete_term_buffer(win_d->visible_tb);
    deinit_window_base(w);
    al_free(win_d->al, win_d);
}

static void win_d_render(window_t *w) {
    window_dummy_t *win_d = (window_dummy_t *)w;

    if (win_d->dirty_buffer) {
        gfx_clear(w->buf, gfx_color(0, 0, 0));
    }

    gfx_draw_term_buffer(
        w->buf, 
        NULL, 
        win_d->dirty_buffer ? NULL : win_d->visible_tb, win_d->real_tb, 
        WINDOW_DUMMY_FONT, WINDOW_DUMMY_PALETTE, 
        0, 0, 1, 1
    );

    const uint16_t cursor_x = WINDOW_DUMMY_FONT->char_width * win_d->real_tb->cursor_col;
    const uint16_t cursor_y = WINDOW_DUMMY_FONT->char_height * win_d->real_tb->cursor_row;

    gfx_fill_rect(
        w->buf, NULL, cursor_x, cursor_y, 
        WINDOW_DUMMY_FONT->char_width, WINDOW_DUMMY_FONT->char_height, 
        WINDOW_DUMMY_PALETTE->colors[w->focused ? TC_WHITE : TC_LIGHT_GREY]
    );

    tb_copy(win_d->visible_tb, win_d->real_tb);
    win_d->dirty_buffer = false;
}

static fernos_error_t win_d_on_event(window_t *w, window_event_t ev) {
    fernos_error_t err;

    window_dummy_t *win_d = (window_dummy_t *)w;

    if (ev.event_code == WINEC_DEREGISTERED) {
        // We are self managed!
        // Delete here!
        delete_window_dummy(w);
        return FOS_E_SUCCESS;
    }

    if (ev.event_code == WINEC_RESIZED) {
        uint16_t new_cols = ev.d.dims.width / WINDOW_DUMMY_FONT->char_width;
        if (new_cols == 0) {
            new_cols = 1;
        }

        uint16_t new_rows = ev.d.dims.height / WINDOW_DUMMY_FONT->char_height;
        if (new_rows == 0) {
            new_rows = 1;
        }

        err = tb_resize(win_d->visible_tb, new_rows, new_cols);
        if (err == FOS_E_SUCCESS) {
            err = tb_resize(win_d->real_tb, new_rows, new_cols);
        }

        if (err != FOS_E_SUCCESS) {
            return err;
        }

        win_d->dirty_buffer = true;

        ev.d.dims.width = new_cols;
        ev.d.dims.height = new_rows;
    }

    // Ticks will be special, because they happen so often, only print every now and then.
    if (ev.event_code == WINEC_TICK && (w->tick % 64)) {
        return FOS_E_SUCCESS; // Early exit for Ticks.
    }

    char log_buf[100]; 
    win_ev_to_str(log_buf, ev, true);

    tb_put_s(win_d->real_tb, log_buf);
    tb_put_c(win_d->real_tb, '\n');

    return FOS_E_SUCCESS;
}
