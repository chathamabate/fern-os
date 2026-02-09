
#include "s_gfx/window.h"
#include "s_util/ansi.h"

const char * const WINEC_NAME_MAP[] = {
    [WINEC_TICK] = "TICK",
    [WINEC_RESIZED] = "RESIZED",
    [WINEC_KEY_INPUT] = "KEY_INPUT",
    [WINEC_DEREGISTERED] = "DEREGISTERED",
    [WINEC_FOCUSED] = "FOCUSED",
    [WINEC_UNFOCUSED] = "UNFOCUSED",
    [WINEC_HIDDEN] = "HIDDEN",
    [WINEC_UNHIDDEN] = "UNHIDDEN",
};

size_t win_ev_to_str(char *buf, window_event_t ev, bool with_ansi) {
    size_t written = 0;

    const char *prefix = with_ansi 
        ? ANSI_BRIGHT_GREEN_FG "[" ANSI_RESET "%s" ANSI_BRIGHT_GREEN_FG "]" ANSI_RESET 
        : "[%s]";

    const char *ev_name = WINEC_NAME_MAP[ev.event_code];
    if (!ev_name) {
        ev_name = "UNKNOWN";
    }

    written += str_fmt(buf + written, prefix, ev_name);

    switch (ev.event_code) {
    case WINEC_RESIZED:
        written += str_fmt(buf + written, " %u %u", ev.d.dims.width, ev.d.dims.height);
        break;

    case WINEC_KEY_INPUT:
        if (with_ansi) {
            const char *kc_fmt = scs1_is_make(ev.d.key_code) 
                ? ANSI_BRIGHT_GREEN_FG " 0x%04X" ANSI_RESET 
                : ANSI_BRIGHT_RED_FG " 0x%04X" ANSI_RESET;
            written += str_fmt(buf + written, kc_fmt, ev.d.key_code);
        } else {
            written += str_fmt(buf + written, " 0x%04X", ev.d.key_code);
        }

        char c = scs1_to_ascii_lc(scs1_as_make(ev.d.key_code));
        if (c) {
            const char key_s[] = {c, '\0'};
            written += str_fmt(buf + written, " \"%s\"", key_s);
        }

        break;

    default:
        break;
    }

    return written;
}

void init_window_base(window_t *w, gfx_buffer_t *buf, const window_attrs_t *attrs, const window_impl_t *impl) {
    *(const window_impl_t **)&(w->impl) = impl;
    *(window_attrs_t *)&(w->attrs) = *attrs;
    *(gfx_buffer_t **)&(w->buf) = buf;
    w->is_active = true;
    w->container = NULL;

    w->focused = false;
    w->hidden = true;
    w->tick = 0;
}

void deinit_window_base(window_t *w) {
    delete_gfx_buffer(w->buf);
}

fernos_error_t win_resize(window_t *w, uint16_t width, uint16_t height) {
    fernos_error_t err;

    if (!(w->is_active)) {
        return FOS_E_INACTIVE;
    }

    if (width < w->attrs.min_width || w->attrs.max_width < width) {
        return FOS_E_NOT_PERMITTED;
    }

    if (height < w->attrs.min_height || w->attrs.max_height < height) {
        return FOS_E_NOT_PERMITTED;
    }

    err = gfx_resize_buffer(w->buf, width, height, false);
    if (err != FOS_E_SUCCESS) {
        return FOS_E_NO_MEM; // regardless of error, return no mem.
    }

    err = w->impl->win_on_event(w, (window_event_t) {
        .event_code = WINEC_RESIZED,
        .d.dims = {
            .width = width,
            .height = height
        }
    });

    if (err != FOS_E_SUCCESS) {
        w->is_active = false;
        return FOS_E_INACTIVE;
    }

    return FOS_E_SUCCESS;
}

fernos_error_t win_fwd_event(window_t *w, window_event_t ev) {
    fernos_error_t err;

    if (!(w->is_active)) {
        return FOS_E_INACTIVE;
    }

    switch (ev.event_code) {

    case WINEC_TICK:
        w->tick++;
        break;

    case WINEC_RESIZED:
        return FOS_E_NOT_PERMITTED;

    case WINEC_DEREGISTERED:
        return FOS_E_NOT_PERMITTED;

    case WINEC_FOCUSED:
        w->focused = true;
        break;

    case WINEC_UNFOCUSED:
        w->focused = false;
        break;

    case WINEC_HIDDEN:
        w->hidden = true;
        break;

    case WINEC_UNHIDDEN:
        w->hidden = false;
        break;

    default:
        break;

    }

    err = w->impl->win_on_event(w, ev);


    if (err != FOS_E_SUCCESS) {
        if (err == FOS_E_INACTIVE) {
            w->is_active = false;
            return FOS_E_INACTIVE;
        }

        // Still active in this case!
        return FOS_E_UNKNWON_ERROR;
    }

    return FOS_E_SUCCESS;
}


fernos_error_t win_register_child(window_t *w, window_t *sw) {
    fernos_error_t err;

    if (!(w->is_active)) {
        return FOS_E_INACTIVE;
    }

    if (!(w->impl->win_register_child)) {
        return FOS_E_NOT_IMPLEMENTED;
    }

    if (sw->container || !(sw->is_active)) {
        return FOS_E_STATE_MISMATCH;
    }

    err = w->impl->win_register_child(w, sw);

    if (err == FOS_E_INACTIVE) {
        w->is_active = false;
        return FOS_E_INACTIVE;
    }

    if (err != FOS_E_SUCCESS) {
        return FOS_E_UNKNWON_ERROR;
    }

    sw->container = w;
    return FOS_E_SUCCESS;
}

void win_deregister(window_t *w) {
    if (!w || !(w->container)) {
        return;
    }

    // Per the docs, we assume that all container windows implement the
    // deregister child endpoint.
    w->container->impl->win_deregister_child(w->container, w);
    w->container = NULL;
    w->impl->win_on_event(w, (window_event_t) {.event_code = WINEC_DEREGISTERED});
}
