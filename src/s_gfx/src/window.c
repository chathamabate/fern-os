
#include "s_gfx/window.h"

void init_window_base(window_t *w, gfx_buffer_t *buf, const window_impl_t *impl) {
    *(const window_impl_t **)&(w->impl) = impl;
    *(gfx_buffer_t **)&(w->buf) = buf;

    w->state = WINSTATE_INVISIBLE;
}

void deinit_window_base(window_t *w) {
    delete_gfx_buffer(w->buf);
}

/**
 * Helper function for entering error state.
 *
 * Does no checks on current state.
 */
static void window_error_out(window_t *w, fernos_error_t error) {
    window_state_t old_state = w->state;

    w->state = WINSTATE_ERRORED_OUT;
    w->error = error;
    w->impl->window_on_state_change(w, old_state);
}

fernos_error_t window_change_state(window_t *w, window_state_t new_state) {
    fernos_error_t err;

    if (w->state == WINSTATE_CLOSED || w->state == WINSTATE_ERRORED_OUT) {
        return FOS_E_NOT_PERMITTED;
    }

    window_state_t old_state = w->state;
    w->state = new_state;

    err = w->impl->window_on_state_change(w, old_state);

    if (err != FOS_E_SUCCESS) {
        window_error_out(w, err);
        return err;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t window_resize(window_t *w, uint16_t width, uint16_t height) {
    fernos_error_t err;

    if (w->state == WINSTATE_CLOSED || w->state == WINSTATE_ERRORED_OUT) {
        return FOS_E_NOT_PERMITTED;
    }

    err = gfx_resize_buffer(w->buf, width, height, false);
    if (err != FOS_E_SUCCESS) {
        window_error_out(w, err);
        return err;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t window_forward_key_input(window_t *w, scs1_code_t key_code) {
    fernos_error_t err;

    if (w->state == WINSTATE_CLOSED || w->state == WINSTATE_ERRORED_OUT) {
        return FOS_E_NOT_PERMITTED;
    }

    err = w->impl->window_on_key_input(w, key_code);
    if (err != FOS_E_SUCCESS) {
        window_error_out(w, err);
        return err;
    }

    return FOS_E_SUCCESS;
}
