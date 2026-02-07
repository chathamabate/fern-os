
#include "s_gfx/window_qgrid.h"

#if WIN_QGRID_BORDER_WIDTH < WIN_QGRID_FOCUS_BORDER_WIDTH
#error "Focused border width must be <= border width!"
#endif

static void delete_window_qgrid(window_t *w);
static void win_qg_render(window_t *w);
static fernos_error_t win_qg_on_event(window_t *w, window_event_t ev);
static fernos_error_t win_qg_register_child(window_t *w, window_t *sw);
static void win_qg_deregister_child(window_t *w, window_t *sw);

static const window_impl_t QGRID_IMPL = {
    .delete_window = delete_window_qgrid,
    .win_render = win_qg_render,
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

    win_qg->lalt_held = false;
    win_qg->ralt_held = false;

    win_qg->lshift_held = false;
    win_qg->rshift_held = false;

    return (window_t *)win_qg;
}

static void delete_window_qgrid(window_t *w) {
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
 * Render a single tile given it's (x, y, w, h)
 * (A focus border will be drawn around/outside this tile if focused)
 */
static void win_qg_render_tile(window_qgrid_t *win_qg, window_t *sw, uint16_t x, uint16_t y, 
        uint16_t w, uint16_t h, bool focused) {
    const gfx_color_t gap_fill_color = WIN_QGRID_BG_COLOR;

    if (sw) {
        // With the new render virtual function design, it is the container's responsibility to call
        // the render function of subwindows!
        win_render(sw); // must do this before pasting into container buffer.

        gfx_box_t clip = {
            .x = x, .y = y, .width = w, .height = h
        };
        gfx_paste_buffer(win_qg->super.buf, &clip, sw->buf, x, y);

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
    } else {
        // Now window to render, just fill a nice big ol' rectangle.
        gfx_fill_rect(win_qg->super.buf, NULL, x, y, w, h, gap_fill_color);
    }

    // Draw focus border regardless of whether `sw` exists.
    if (focused) {
        gfx_color_t focus_border_color;

        if (win_qg->lalt_held || win_qg->ralt_held) {
            if (win_qg->lshift_held || win_qg->rshift_held) {
                focus_border_color = WIN_QGRID_FOCUS_MOV_BORDER_COLOR;
            } else {
                focus_border_color = WIN_QGRID_FOCUS_ALT_BORDER_COLOR;
            }
        } else {
            focus_border_color = WIN_QGRID_FOCUS_BORDER_COLOR;
        }
 
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

static void win_qg_render(window_t *w) {
    window_qgrid_t * const win_qg = (window_qgrid_t *)w;
    gfx_buffer_t * const buf = win_qg->super.buf;

    const gfx_color_t border_color = WIN_QGRID_BORDER_COLOR;

    // We are going to draw the exterior border 1 pixel thicker, this way, when we are in grid 
    // mode, there is no possibility of issues with divison by 2 round off.
    // (This only really works because we are rendering a 2x2 grid.)
    gfx_draw_rect(buf, NULL, 0, 0, buf->width, buf->height, 
            WIN_QGRID_BORDER_WIDTH + 1, border_color);

    if (win_qg->single_pane_mode) { // single pane mode is simplest.
        win_qg_render_tile(win_qg, 
            win_qg->grid[win_qg->focused_row][win_qg->focused_col], 
            WIN_QGRID_BORDER_WIDTH, WIN_QGRID_BORDER_WIDTH, 
            win_qg_large_tile_width(win_qg),
            win_qg_large_tile_height(win_qg),
            win_qg->super.focused
        ); 
    } else {
        const uint16_t tile_width = win_qg_tile_width(win_qg);
        const uint16_t tile_height = win_qg_tile_height(win_qg);

        // Here we draw a cross with center overlap.
        gfx_fill_rect(buf, NULL, 
            WIN_QGRID_BORDER_WIDTH + tile_width, WIN_QGRID_BORDER_WIDTH, 
            WIN_QGRID_BORDER_WIDTH, buf->height - (2 * WIN_QGRID_BORDER_WIDTH), 
            border_color
        );

        gfx_fill_rect(buf, NULL,
            WIN_QGRID_BORDER_WIDTH,WIN_QGRID_BORDER_WIDTH + tile_height,
            buf->width - (2 * WIN_QGRID_BORDER_WIDTH), WIN_QGRID_BORDER_WIDTH,
            border_color
        );

        for (size_t r = 0; r < 2; r++) {
            for (size_t c = 0; c < 2; c++) {
                window_t *sw = win_qg->grid[r][c];
                win_qg_render_tile(win_qg, sw, 
                    WIN_QGRID_BORDER_WIDTH + (c * (tile_width + WIN_QGRID_BORDER_WIDTH)), 
                    WIN_QGRID_BORDER_WIDTH + (r * (tile_height + WIN_QGRID_BORDER_WIDTH)),
                    tile_width, tile_height, 
                    win_qg->super.focused && r == win_qg->focused_row && c == win_qg->focused_col
                );
            }
        }
    }
}

/**
 * Move the focused pane to position (r, c).
 * Does nothing if (r, c) is invalid!
 *
 * NOTE: This doesn't send any events at all, IT DOESN"T NEED TO!
 */
static void win_qg_move_focused_pane(window_qgrid_t *win_qg, size_t r, size_t c) {
    if (r >= 2 || c >= 2) { // Invalid position.
        return;
    }

    if (r == win_qg->focused_row && c == win_qg->focused_col) { // Nothing needs to be done here.
        return;
    }

    window_t *temp = win_qg->grid[r][c];
    win_qg->grid[r][c] = win_qg->grid[win_qg->focused_row][win_qg->focused_col];
    win_qg->grid[win_qg->focused_row][win_qg->focused_col] = temp;

    win_qg->focused_row = r;
    win_qg->focused_col = c;

    // Realize this doesn't need to send any events!
    // The focused window remains focused. 
    //
    // No new windows become hidden/unhidden!
}

#include "k_startup/gfx.h"

/**
 * Set the focus position of the window.
 *
 * Does nothing if (r, c) is invalid or already focused.
 * In single pane mode, this will shrink the current focused window, and expand the newly
 * focused window!
 */
static void win_qg_set_focus_position(window_qgrid_t *win_qg, size_t r, size_t c) {
    fernos_error_t err;

    if (r >= 2 || c >= 2) { // Invalid position.
        return;
    }

    if (r == win_qg->focused_row && c == win_qg->focused_col) { // already focused.
        return;
    }

    window_t *init_focus = win_qg->grid[win_qg->focused_row][win_qg->focused_col];
    window_t *next_focus = win_qg->grid[r][c];

    // We are going to move!

    if (win_qg->single_pane_mode) {
        // In single pane mode, we always shrink current window back to correct tile size.        
        if (init_focus) {
            err = win_resize(init_focus, 
                    win_qg_tile_width(win_qg),
                    win_qg_tile_height(win_qg)
            );

            // This initally focused window is now off screen.
            if (err != FOS_E_INACTIVE) {
                err = win_fwd_event(init_focus, (window_event_t) { .event_code = WINEC_HIDDEN });
            }

            if (err == FOS_E_INACTIVE) {
                win_deregister(init_focus);
                init_focus = NULL; // something went wrong with the initally focused window.
                                   // remove all references.
            }
        }

        if (next_focus) {
            err = win_resize(next_focus, 
                    win_qg_large_tile_width(win_qg),
                    win_qg_large_tile_height(win_qg)
            );

            // The next focused window is now on screen!
            if (err != FOS_E_INACTIVE) {
                err = win_fwd_event(next_focus, (window_event_t) { .event_code = WINEC_UNHIDDEN });
            }

            if (err == FOS_E_INACTIVE) {
                win_deregister(next_focus);
                next_focus = NULL; // something went wrong with the next focused window.
                                   // remove all references!
            }
        }
    }

    // In grid mode, all subwindows will have identical size, no need to resize anything.

    // Now we always send focus/unfocus events to init/next.

    if (init_focus) {
        // Realize, that here `init_focus` will be forwarded an unfocus event AFTER it may have
        // been hidden. This is OK! Window implementations should not expect an ordering to
        // focused/unfocused/hidden/unhidden events!
        err = win_fwd_event(init_focus, (window_event_t) {.event_code = WINEC_UNFOCUSED});
        if (err == FOS_E_INACTIVE) {
            win_deregister(init_focus);
            init_focus = NULL;
        }
    }

    if (next_focus) {
        err = win_fwd_event(next_focus, (window_event_t) {.event_code = WINEC_FOCUSED});
        if (err == FOS_E_INACTIVE) {
            win_deregister(next_focus);
            next_focus = NULL;
        }
    }

    win_qg->focused_row = r;
    win_qg->focused_col = c;
}

static fernos_error_t win_qg_on_event(window_t *w, window_event_t ev) {
    fernos_error_t err;
    window_t *sw;

    window_qgrid_t *win_qg = (window_qgrid_t *)w;

    switch (ev.event_code) {

    case WINEC_TICK: {
        // We forward a tick to all subwindows always!
        for (size_t r = 0; r < 2; r++) {
            for (size_t c = 0; c < 2; c++) {
                sw = win_qg->grid[r][c];
                if (sw) {
                    err = win_fwd_event(sw, (window_event_t) {.event_code = WINEC_TICK});
                    if (err == FOS_E_INACTIVE) {
                        win_deregister(sw);
                    }
                }
            }
        }

        // We used to render here before we had a separate virtual render endpoint!

        return FOS_E_SUCCESS;
    }

    case WINEC_RESIZED: {
        if (win_qg->single_pane_mode) {
            // In single pane mode we just resize the visible pane!
            sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col]; 
            if (sw) {
                err = win_resize(sw, 
                        win_qg_large_tile_width(win_qg),
                        win_qg_large_tile_height(win_qg)
                );
                if (err == FOS_E_INACTIVE) {
                    win_deregister(sw);
                }
            }
            
            return FOS_E_SUCCESS;
        }

        const uint16_t tile_width = win_qg_tile_width(win_qg);
        const uint16_t tile_height = win_qg_tile_height(win_qg);

        // For grid mode let's try and resize each individual tile.
        for (size_t r = 0; r < 2; r++) {
            for (size_t c = 0; c < 2; c++) {
                sw = win_qg->grid[r][c];
                if (sw) {
                    err = win_resize(sw, tile_width, tile_height);
                    if (err == FOS_E_INACTIVE) {
                        win_deregister(sw);
                    }
                }
            }
        }

        return FOS_E_SUCCESS;
    }

    case WINEC_KEY_INPUT: {

        // Alright, so, if control is not held, all key codes get forwarded immediately
        // to the focused window.

        const scs1_code_t kc = ev.d.key_code;
        const scs1_code_t make_code = scs1_as_make(kc);
        const bool is_make = scs1_is_make(kc);

        switch (make_code) {
        case SCS1_LALT:
            win_qg->lalt_held = is_make;
            return FOS_E_SUCCESS;

        case SCS1_E_RALT:
            win_qg->ralt_held = is_make;
            return FOS_E_SUCCESS;

        case SCS1_LSHIFT:
            win_qg->lshift_held = is_make;
            return FOS_E_SUCCESS;

        case SCS1_RSHIFT:
            win_qg->lshift_held = is_make;
            return FOS_E_SUCCESS;

        default:
            break;
        }

        if (!(win_qg->lalt_held || win_qg->ralt_held)) { // arbitrary character forward!
            sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col];
            if (sw) {
                err = win_fwd_event(sw, (window_event_t) {
                    .event_code = WINEC_KEY_INPUT,
                    .d.key_code = kc
                });
                if (err == FOS_E_INACTIVE) {
                    win_deregister(sw);
                }
            }

            return FOS_E_SUCCESS;
        }

        // Control sequence!

        switch (kc) { // we use the raw key code here because we only care about key presses.

        case SCS1_E_UP: {
            if (win_qg->lshift_held || win_qg->rshift_held) {
                win_qg_move_focused_pane(win_qg, win_qg->focused_row - 1, win_qg->focused_col);
            } else {
                win_qg_set_focus_position(win_qg, win_qg->focused_row - 1, win_qg->focused_col);
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_DOWN: {
            if (win_qg->lshift_held || win_qg->rshift_held) {
                win_qg_move_focused_pane(win_qg, win_qg->focused_row + 1, win_qg->focused_col);
            } else {
                win_qg_set_focus_position(win_qg, win_qg->focused_row + 1, win_qg->focused_col);
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_LEFT: {
            if (win_qg->lshift_held || win_qg->rshift_held) {
                win_qg_move_focused_pane(win_qg, win_qg->focused_row, win_qg->focused_col - 1);
            } else {
                win_qg_set_focus_position(win_qg, win_qg->focused_row, win_qg->focused_col - 1);
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_E_RIGHT: {
            if (win_qg->lshift_held || win_qg->rshift_held) {
                win_qg_move_focused_pane(win_qg, win_qg->focused_row, win_qg->focused_col + 1);
            } else {
                win_qg_set_focus_position(win_qg, win_qg->focused_row, win_qg->focused_col + 1);
            }
            return FOS_E_SUCCESS;
        }

        case SCS1_F: {
            // Toggle single pane mode.
            win_qg->single_pane_mode = !(win_qg->single_pane_mode);

            sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col];
            if (sw) { // If there is a focused window, we must resize it!
                const size_t new_width = win_qg->single_pane_mode 
                    ? win_qg_large_tile_width(win_qg) : win_qg_tile_width(win_qg);

                const size_t new_height = win_qg->single_pane_mode 
                    ? win_qg_large_tile_height(win_qg) : win_qg_tile_height(win_qg);

                err = win_resize(sw, new_width, new_height);
                if (err == FOS_E_INACTIVE) {
                    win_deregister(sw);
                }
            }

            // Now, for all sub windows which are NOT in the focus position.
            // They either become hidden or unhidden!

            for (size_t r = 0; r < 2; r++) {
                for (size_t c = 0; c < 2; c++) {
                    if (r == win_qg->focused_row && c == win_qg->focused_col) {
                        continue;
                    }

                    sw = win_qg->grid[r][c];
                    if (sw) {
                        err = win_fwd_event(sw, (window_event_t) {
                            .event_code = win_qg->single_pane_mode 
                                ? WINEC_HIDDEN      // We are entering single pane mode, everyone hide!
                                : WINEC_UNHIDDEN    // We are exiting single pane mode, everyone come out!
                        });
                        if (err == FOS_E_INACTIVE) {
                            win_deregister(sw);
                        }
                    }
                }
            }

            return FOS_E_SUCCESS;
        }

        case SCS1_X: { // Deregister focused window.
            sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col];
            if (sw) {
                win_deregister(sw);
            }
            return FOS_E_SUCCESS;
        }

        default: { // Unmapped controls sequences are ignored!
            return FOS_E_SUCCESS;
        }

        }

        // Consider a  forced rerender instead of all key cases returning?
    }

    case WINEC_DEREGISTERED: {
        return FOS_E_SUCCESS;
    }

    /*
     * When the entire window is focused, a focus event is sent to the internally focused 
     * subwindow. (Same goes for unfocused)
     */
    case WINEC_FOCUSED: 
    case WINEC_UNFOCUSED: {
        sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col]; 
        if (sw) {
            err = win_fwd_event(sw, (window_event_t) { .event_code = ev.event_code });
            if (err == FOS_E_INACTIVE) {
                win_deregister(sw);
            }
        }

        return FOS_E_SUCCESS;
    }

    case WINEC_HIDDEN: 
    case WINEC_UNHIDDEN: {
        // In single pane mode, only the internally focused window's state will change on
        // hide/unhide. 
        if (win_qg->single_pane_mode) {
            sw = win_qg->grid[win_qg->focused_row][win_qg->focused_col]; 
            if (sw) {
                err = win_fwd_event(sw, (window_event_t) { .event_code = ev.event_code });
                if (err == FOS_E_INACTIVE) {
                    win_deregister(sw);
                }
            }

            return FOS_E_SUCCESS;
        }

        // Otherwise, all panes are affected!
        for (size_t r = 0; r < 2; r++) {
            for (size_t c = 0; c < 2; c++) {
                sw = win_qg->grid[r][c];
                if (sw) {
                    err = win_fwd_event(sw, (window_event_t) { .event_code = ev.event_code });
                    if (err == FOS_E_INACTIVE) {
                        win_deregister(sw);
                    }
                }
            }
        }

        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_SUCCESS;
    }

    }
}

static fernos_error_t win_qg_register_child(window_t *w, window_t *sw) {
    fernos_error_t err;
    window_qgrid_t *win_qg = (window_qgrid_t *)w;

    // Here we will look for an open spot!

    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            if (!(win_qg->grid[r][c])) { // We found a spot!
                // Very important, `win_qg_set_focus_position` potentially deregisters the window
                // at (r, c). `sw` is not in a valid state to be deregistered at this time!
                // So, it's important we set the focus position, before referencing `sw` in
                // the grid.
                win_qg_set_focus_position(win_qg, r, c);

                // Let's attempt a resize. 
                const size_t new_width = win_qg->single_pane_mode 
                    ? win_qg_large_tile_width(win_qg) : win_qg_tile_width(win_qg);

                const size_t new_height = win_qg->single_pane_mode 
                    ? win_qg_large_tile_height(win_qg) : win_qg_tile_height(win_qg);

                err = win_resize(sw, new_width, new_height);

                // When a window is registered, it always become the focused pane, which for
                // a qgrid window means it is also unhidden!
                if (err != FOS_E_INACTIVE) {
                    err = win_fwd_event(sw, (window_event_t) {.event_code = WINEC_UNHIDDEN});
                }

                if (err != FOS_E_INACTIVE) {
                    err = win_fwd_event(sw, (window_event_t) {.event_code = WINEC_FOCUSED});
                }

                if (err == FOS_E_INACTIVE) {
                    return FOS_E_UNKNWON_ERROR; // `sw` broke during focus?
                }

                // Success!
                win_qg->grid[r][c] = sw;

                return FOS_E_SUCCESS;
            }
        }
    }

    return FOS_E_NO_SPACE;
}

static void win_qg_deregister_child(window_t *w, window_t *sw) {
    window_qgrid_t *win_qg = (window_qgrid_t *)w;

    // Here we just search for `sw` in the grid, dereferencing it if found!

    for (size_t r = 0; r < 2; r++) {
        for (size_t c = 0; c < 2; c++) {
            if (win_qg->grid[r][c] == sw) {
                win_qg->grid[r][c] = NULL; 
                return;
            }
        }
    }
}


