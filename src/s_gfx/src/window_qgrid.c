
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

static void win_qg_render(window_qgrid_t *win_qg) {
    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            window_t *sw = win_qg->grid[r][c];

            // Buffer in buffer tbh.
            if (sw) {
                
            } else {

            }
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


