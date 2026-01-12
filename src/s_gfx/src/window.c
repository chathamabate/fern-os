
#include "s_gfx/window.h"

void init_window_base(window_t *w, gfx_buffer_t *buf, const window_impl_t *impl) {
    *(const window_impl_t **)&(w->impl) = impl;
    *(gfx_buffer_t **)&(w->buf) = buf;

    w->state = WINSTATE_INVISIBLE;
}

void deinit_window_base(window_t *w) {
    delete_gfx_buffer(w->buf);
}

window_state_t win_change_state(window_t *w, window_state_t new_state) {
    if (w->state == WINSTATE_CLOSED || new_state == w->state) {
        return w->state;
    }

    if (new_state == WINSTATE_CLOSED) {
        return win_close(w, FOS_E_SUCCESS);
    }

    window_state_t old_state = w->state;
    w->state = new_state;

    if (w->impl->win_on_state_change) {
        w->impl->win_on_state_change(w, old_state);
    }

    return old_state;
}

window_state_t win_close(window_t *w, fernos_error_t close_status) {
    if (w->state == WINSTATE_CLOSED) {
        return w->state;
    }

    window_state_t old_state = w->state;

    w->state = WINSTATE_CLOSED;
    w->close_status = close_status;

    if (w->impl->win_on_state_change) {
        w->impl->win_on_state_change(w, old_state);
    }

    return old_state;
}

window_state_t win_resize(window_t *w, uint16_t width, uint16_t height) {
    fernos_error_t err;

    if (w->state == WINSTATE_CLOSED) {
        return w->state;
    }

    err = gfx_resize_buffer(w->buf, width, height, false);
    if (err != FOS_E_SUCCESS) {
        return win_close(w, err);
    }

    window_state_t old_state = w->state;

    if (w->impl->win_on_resize) {
        w->impl->win_on_resize(w);
    }

    return old_state;
}


window_state_t win_forward_key_input(window_t *w, scs1_code_t key_code) {
    if (w->state == WINSTATE_CLOSED) {
        return w->state;
    }

    window_state_t old_state = w->state;

    if (w->impl->win_on_key_input) {
        w->impl->win_on_key_input(w, key_code);
    }

    return old_state;
}

window_state_t win_tick(window_t *w) {
    if (w->state == WINSTATE_CLOSED) {
        return w->state;
    }

    window_state_t old_state = w->state;

    if (w->impl->win_on_tick) {
        w->impl->win_on_tick(w);
    }

    return old_state;
}
