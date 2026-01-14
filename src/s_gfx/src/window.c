
#include "s_gfx/window.h"

void init_window_base(window_t *w, gfx_buffer_t *buf, const window_impl_t *impl) {
    *(const window_impl_t **)&(w->impl) = impl;
    *(gfx_buffer_t **)&(w->buf) = buf;
    w->fatal_error_encountered = false;
    w->container = NULL;
}

void deinit_window_base(window_t *w) {
    delete_gfx_buffer(w->buf);
}

fernos_error_t win_resize(window_t *w, uint16_t width, uint16_t height) {
    fernos_error_t err;

    if (w->fatal_error_encountered) {
        return FOS_E_FATAL;
    }

    err = gfx_resize_buffer(w->buf, width, height, false);
    if (err != FOS_E_SUCCESS) {
        return FOS_E_NO_MEM; // regardless of error, return no mem.
    }

    if (w->impl->win_on_resize) {
        err = w->impl->win_on_resize(w);
        if (err != FOS_E_SUCCESS) {
            w->fatal_error_encountered = true;
            return FOS_E_FATAL;
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t win_on_key_input(window_t *w, scs1_code_t key_code) {
    fernos_error_t err;

    if (w->fatal_error_encountered) {
        return FOS_E_FATAL;
    }

    if (w->impl->win_on_key_input) {
        err = w->impl->win_on_key_input(w, key_code);

        if (err == FOS_E_FATAL) {
            w->fatal_error_encountered = true;
            return FOS_E_FATAL;
        }

        return err;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t win_tick(window_t *w) {
    fernos_error_t err;

    if (w->fatal_error_encountered) {
        return FOS_E_FATAL;
    }

    if (w->impl->win_tick) {
        err = w->impl->win_tick(w);

        if (err == FOS_E_FATAL) {
            w->fatal_error_encountered = true;
            return FOS_E_FATAL;
        }

        return err;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t win_register_child(window_t *w, window_t *sw) {
    fernos_error_t err;

    if (w->fatal_error_encountered) {
        return FOS_E_FATAL;
    }

    if (!(w->impl->win_register_child)) {
        return FOS_E_NOT_IMPLEMENTED;
    }

    if (!sw) {
        return FOS_E_BAD_ARGS;
    }

    if (sw->container) {
        return FOS_E_STATE_MISMATCH;
    }

    err = w->impl->win_register_child(w, sw);

    if (err == FOS_E_FATAL) {
        w->fatal_error_encountered = true;
        return FOS_E_FATAL;
    }

    if (err == FOS_E_SUCCESS) {
        sw->container = w;
        return FOS_E_SUCCESS;
    }

    return err;
}

void win_deregister_child(window_t *w, window_t *sw) {
    if (!sw || sw->container != w) {
        return;
    }

    if (w->fatal_error_encountered) {
        return;
    }

    if (w->impl->win_deregister_child) {
        w->impl->win_deregister_child(w, sw);
    }

    sw->container = NULL;
    if (sw->impl->win_on_deregister) {
        sw->impl->win_on_deregister(sw);
    }
}
