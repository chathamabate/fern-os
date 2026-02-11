
#include "k_startup/plugin_gfx.h"
#include "k_startup/gfx.h"
#include "s_gfx/window_dummy.h"
#include "k_startup/page_helpers.h"
#include "s_util/ansi.h"
#include "os_defs.h"

static window_terminal_t *new_window_terminal(allocator_t *al, uint16_t rows, uint16_t cols, 
        const gfx_term_buffer_attrs_t *attrs, ring_t *sch);
static void delete_terminal_window(window_t *w);
static void tw_render(window_t *w);
static fernos_error_t tw_on_event(window_t *w, window_event_t ev);

static const window_impl_t TERMINAL_WINDOW_IMPL = {
    .delete_window = delete_terminal_window,
    .win_render = tw_render,
    .win_on_event = tw_on_event,

    // Terminal windows never contain subwindows.
    .win_register_child = NULL,
    .win_deregister_child = NULL
};

static window_terminal_t *new_window_terminal(allocator_t *al, uint16_t rows, uint16_t cols, 
        const gfx_term_buffer_attrs_t *attrs, ring_t *sch) {
    if (!al || !attrs || !sch) {
        return NULL;
    }

    if (rows == 0 || cols == 0) {
        return NULL;
    }

    const ascii_mono_font_t * const font = ASCII_MONO_FONT_MAP[attrs->fmi];
    if (!font) {
        return NULL;
    }

    // Calc initial width and height.
    const uint16_t width = cols * font->char_width * attrs->w_scale;
    const uint16_t height = rows * font->char_height * attrs->h_scale;

    window_terminal_t *win_t = al_malloc(al, sizeof(window_terminal_t));
    gfx_buffer_t *buf = new_gfx_buffer(al, width, height);
    term_buffer_t *vis_tb = new_term_buffer(al, (term_cell_t) {
        .c = ' ',
        .style = term_style(TC_WHITE, TC_BLACK)
    }, rows, cols);
    term_buffer_t *true_tb = new_term_buffer(al, (term_cell_t) {
        .c = ' ',
        .style = term_style(TC_WHITE, TC_BLACK)
    }, rows, cols);
    fixed_queue_t *event_queue = new_fixed_queue(al, sizeof(window_event_t), 255);
    basic_wait_queue_t *wq = new_basic_wait_queue(al);

    if (!win_t || !buf || !vis_tb || !true_tb || !event_queue || !wq) {
        delete_wait_queue((wait_queue_t *)wq);
        delete_fixed_queue(event_queue);
        delete_term_buffer(true_tb);
        delete_term_buffer(vis_tb);
        delete_gfx_buffer(buf);
        al_free(al, win_t);

        return NULL;
    }

    // Success!

    const window_attrs_t win_attrs = (window_attrs_t) {
        .min_width = 0,
        .max_width = UINT16_MAX,
        .min_height = 0,
        .max_height = UINT16_MAX
    };

    init_window_base((window_t *)win_t, buf, &win_attrs, &TERMINAL_WINDOW_IMPL);
    *(allocator_t **)&(win_t->al) = al;
    win_t->references = 0;
    *(gfx_term_buffer_attrs_t *)&(win_t->tb_attrs) = *attrs;
    *(term_buffer_t **)&(win_t->visible_tb) = vis_tb;
    win_t->dirty_buffer = true;
    *(term_buffer_t **)&(win_t->true_tb) = true_tb;
    *(fixed_queue_t **)&(win_t->event_queue) = event_queue;
    *(basic_wait_queue_t **)&(win_t->wq) = wq;
    *(ring_t **)&(win_t->schedule) = sch;

    return win_t;
}

/**
 * This deletes the terminal regardless of referece count and wait queue state!
 * So be careful!
 */
static void delete_terminal_window(window_t *w) {
    window_terminal_t *win_t = (window_terminal_t *)w;

    delete_wait_queue((wait_queue_t *)(win_t->wq));
    delete_fixed_queue(win_t->event_queue);
    delete_term_buffer(win_t->true_tb);
    delete_term_buffer(win_t->visible_tb);
    deinit_window_base(w);

    al_free(win_t->al, win_t);
}

/**
 * Rate at which cursor flashes.
 */
#define TW_CURSOR_FLASH_RATE (16U)

/**
 * The height of the flashing cursor bar will be (cell_height / this value)
 */
#define TW_CURSOR_HEIGHT_FRACTION (4U)

static void tw_render(window_t *w) {
    window_terminal_t *win_t = (window_terminal_t *)w;

    const ascii_mono_font_t * const font = ASCII_MONO_FONT_MAP[win_t->tb_attrs.fmi];

    const uint32_t cell_width = win_t->tb_attrs.w_scale * font->char_width;
    const uint32_t cell_height = win_t->tb_attrs.h_scale * font->char_height;
    const uint32_t cursor_height = cell_height / TW_CURSOR_HEIGHT_FRACTION;

    const uint32_t tb_width = cell_width * win_t->visible_tb->cols;
    const uint32_t tb_height = cell_height * win_t->visible_tb->rows;

    // The actual character grid will always be centered!
    const uint32_t tb_x = tb_width < win_t->super.buf->width ? (win_t->super.buf->width - tb_width) / 2 : 0;
    const uint32_t tb_y = tb_height < win_t->super.buf->height ? (win_t->super.buf->height - tb_height) / 2 : 0;

    const uint32_t true_cursor_x = tb_x + (win_t->true_tb->cursor_col * cell_width);
    const uint32_t true_cursor_y = tb_y + (win_t->true_tb->cursor_row * cell_height);

    if (win_t->dirty_buffer) {
        // In case of a dirty buffer, we have to redraw everything!

        gfx_clear(win_t->super.buf, win_t->tb_attrs.palette.colors[TC_LIGHT_GREY]);
        gfx_draw_term_buffer_wa(win_t->super.buf, NULL, NULL, win_t->true_tb, tb_x, tb_y, 
                &(win_t->tb_attrs));

        win_t->dirty_buffer = false;
    } else {
        // Here we just redraw cells which have changed! 
        // (This redraws the visible cursor cell everytime!)
        gfx_draw_term_buffer_wa(win_t->super.buf, NULL, win_t->visible_tb, win_t->true_tb, 
                tb_x, tb_y, &(win_t->tb_attrs));
    }

    // Now actual cursor drawing logic!

    gfx_color_t cursor_color;

    if (win_t->super.focused) {
        cursor_color = (win_t->super.tick / TW_CURSOR_FLASH_RATE) & 1 
            ? win_t->tb_attrs.palette.colors[TC_WHITE] 
            : win_t->tb_attrs.palette.colors[TC_BRIGHT_BROWN];
    } else {
        // No flash when unfocused.
        cursor_color = win_t->tb_attrs.palette.colors[TC_LIGHT_GREY];
    }

    gfx_fill_rect(win_t->super.buf, NULL, true_cursor_x, true_cursor_y + (cell_height - cursor_height),
            cell_width, cursor_height, cursor_color);

    tb_copy(win_t->visible_tb, win_t->true_tb);
}

/**
 * When a terminal window receives an event, the incoming event is placed in the event
 * queue. If threads were waiting for the event-queue to be non-empty, then said threads are
 * woken up.
 *
 * There are some special cases though.
 *
 * WINEC_RESIZED
 * In this case the base window buffer has been resized. On this event, the terminal window
 * must also attempt to resize its two terminal buffers. 
 * The resize event which is then queued up is first modified such that `width` is the new
 * number of columns in the terminal window, and `height` is the new number of rows in the terminal
 * window.
 *
 * WINEC_DEREGISTERED
 * In case of a deregister event, i.e., this window is no longer visible.
 * This window becomes inactive. If there are no open references, this window will delete itself!
 */
static fernos_error_t tw_on_event(window_t *w, window_event_t ev) {
    window_terminal_t *win_t = (window_terminal_t *)w;

    fernos_error_t err;

    const ascii_mono_font_t * const font = ASCII_MONO_FONT_MAP[win_t->tb_attrs.fmi];
    const uint16_t font_width = font->char_width * win_t->tb_attrs.w_scale;
    const uint16_t font_height = font->char_height * win_t->tb_attrs.h_scale;

    switch (ev.event_code) {

    case WINEC_RESIZED: {
        // We CEIL the new rows and columns value as a terminal buffer must always have at least
        // 1 cell.
        uint16_t new_cols = ev.d.dims.width / font_width;
        if (new_cols == 0) {
            new_cols = 1;
        }

        uint16_t new_rows = ev.d.dims.height / font_height;     
        if (new_rows == 0) {
            new_rows = 1;
        }

        err = tb_resize(win_t->visible_tb, new_rows, new_cols);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        err = tb_resize(win_t->true_tb, new_rows, new_cols);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        // After a resize, it is important to note the the frame buffer of this
        // window is in an undefined state!

        win_t->dirty_buffer = true;

        // Finally, overwrite the original event dimmensions.

        ev.d.dims.width = new_cols;
        ev.d.dims.height = new_rows;

        break;
    }

    case WINEC_DEREGISTERED: {
        // We'll say that a terminal window is inactive once deregistered.
        // It'll have to wait until all references go down to zero to cleanup though.
        win_t->super.is_active = false;

        // This window will delete itself on deregister ONLY if there are no open handles!
        if (win_t->references == 0) {
            delete_window((window_t *)win_t);

            // It's ok to return here because 0 references gaurantees no threads in the wait queue!
            // No one to wake up!
            return FOS_E_SUCCESS;
        }

        break;
    }

    case WINEC_TICK: {
        // Ignore tick events!
        return FOS_E_SUCCESS;
    }

    default: {
        break;
    }

    }

    // Regardless of event type, we always place on the queue!

    // If the queue is currently empty, we must wake up all sleeping threads after enqueueing.
    const bool wake_up = fq_is_empty(win_t->event_queue);
    fq_enqueue(win_t->event_queue, &ev, true);

    if (wake_up) {
        err = bwq_wake_all_threads(win_t->wq, win_t->schedule, FOS_E_SUCCESS);
        if (err != FOS_E_SUCCESS) {
            return FOS_E_INACTIVE; // failure to wake threads is a catastrophic error for this
                                   // window, unsure why `bwq_wake_all` even throws an error tbh.
                                   // I dug into the impl, and this error will never happen.
        }
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t copy_handle_terminal_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_handle_terminal_state(handle_state_t *hs);
static fernos_error_t term_hs_wait_write_ready(handle_state_t *hs);
static fernos_error_t term_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
static fernos_error_t term_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const handle_state_impl_t HS_TERM_IMPL = {
    .copy_handle_state = copy_handle_terminal_state,
    .delete_handle_state = delete_handle_terminal_state,

    .hs_wait_write_ready = term_hs_wait_write_ready,
    .hs_write = term_hs_write,

    .hs_wait_read_ready = NULL,
    .hs_read = NULL,

    .hs_cmd = term_hs_cmd
};

static fernos_error_t copy_handle_terminal_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    handle_terminal_state_t *hs_t = (handle_terminal_state_t *)hs;
    handle_terminal_state_t *hs_t_copy = al_malloc(hs_t->super.ks->al, sizeof(handle_terminal_state_t));

    if (!hs_t_copy) {
        return FOS_E_NO_MEM;
    }

    init_base_handle((handle_state_t *)hs_t_copy, &HS_TERM_IMPL, hs_t->super.ks, proc, hs_t->super.handle, true);
    *(window_terminal_t **)&(hs_t_copy->win_t) = hs_t->win_t;

    hs_t_copy->win_t->references++;

    *out = (handle_state_t *)hs_t_copy;

    return FOS_E_SUCCESS;
}

static fernos_error_t delete_handle_terminal_state(handle_state_t *hs) {
    handle_terminal_state_t *hs_t = (handle_terminal_state_t *)hs;

    hs_t->win_t->references--;
    if (hs_t->win_t->references == 0 && !(hs_t->win_t->super.is_active)) {
        // If the underlying terminal window no longer has any references AND is inactive,
        // we can delete the window here too!
        delete_window((window_t *)(hs_t->win_t));
    }

    // Always delete the handle state itself.

    al_free(hs_t->super.ks->al, hs_t);

    return FOS_E_SUCCESS;
}

static fernos_error_t term_hs_wait_write_ready(handle_state_t *hs) {
    handle_terminal_state_t *hs_t = (handle_terminal_state_t *)hs;
    thread_t *thr = (thread_t *)(hs_t->super.ks->schedule.head);

    // Inactive terminal windows never accept more data!
    if (!(hs_t->win_t->super.is_active)) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

/**
 * Maximum number of characters writeable in one go!
 */
#define TERM_MAX_SINGLE_TX_LEN (511U)

static fernos_error_t term_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    fernos_error_t err;
    handle_terminal_state_t *hs_t = (handle_terminal_state_t *)hs;
    thread_t *thr = (thread_t *)(hs_t->super.ks->schedule.head);

    if (!(hs_t->win_t->super.is_active)) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    if (!u_src || len == 0) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    char buf[TERM_MAX_SINGLE_TX_LEN + 1]; // +1 for the NT!

    const size_t bytes_to_cpy = MIN(len, TERM_MAX_SINGLE_TX_LEN);
    err = mem_cpy_from_user(buf, thr->proc->pd, u_src, bytes_to_cpy, NULL);
    if (err != FOS_E_SUCCESS) { // We'll only continue if there is a full success during the copy.
        DUAL_RET(thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
    }

    buf[bytes_to_cpy] = '\0';

    size_t bytes_to_write;

    // NOTE: We only call ansi_trim when we copied less bytes than were requested.
    // This is the only case where we care about potentially slicing an ansi sequence.
    if (bytes_to_cpy < len) { 
        bytes_to_write = ansi_trim(buf);
    } else {
        bytes_to_write = bytes_to_cpy;
    }

    // Finally write to our buffer!

    tb_put_s(hs_t->win_t->true_tb, buf);

    if (u_written) {
        err = mem_cpy_to_user(thr->proc->pd, u_written, &bytes_to_write, sizeof(size_t), NULL);
        if (err != FOS_E_SUCCESS) { 
            DUAL_RET(thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
        }
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

/**
 * The maximum number of events we can read in a single read!
 */
#define TERM_WIN_HS_MAX_EVS_PER_READ (32U)

static fernos_error_t term_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;
    handle_terminal_state_t *hs_t = (handle_terminal_state_t *)hs;
    thread_t *thr = (thread_t *)(hs_t->super.ks->schedule.head);

    (void)arg3;

    switch (cmd) {

    case TERM_HCID_GET_DIMS: {
        size_t *u_rows = (size_t *)arg0;
        size_t *u_cols = (size_t *)arg1;

        if (u_rows) {
            const size_t rows = hs_t->win_t->true_tb->rows;
            err = mem_cpy_to_user(thr->proc->pd, u_rows, &rows, sizeof(size_t), NULL);
            if (err != FOS_E_SUCCESS) {
                DUAL_RET(thr, err, FOS_E_SUCCESS);
            }
        }
        
        if (u_cols) {
            const size_t cols = hs_t->win_t->true_tb->cols;
            err = mem_cpy_to_user(thr->proc->pd, u_cols, &cols, sizeof(size_t), NULL);
            if (err != FOS_E_SUCCESS) {
                DUAL_RET(thr, err, FOS_E_SUCCESS);
            }
        }

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    case TERM_HCID_WAIT_EVENT: {
        // Already stuff in the wait queue!
        if (!fq_is_empty(hs_t->win_t->event_queue)) {
            DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
        }

        // When the terminal window is inactive, we'll allow the user to read
        // DEREGISTERED Events infinitely.
        if (!(hs_t->win_t->super.is_active)) {
            DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
        }

        // Active with empty event queue!
        // Wait!
    
        basic_wait_queue_t *bwq = hs_t->win_t->wq;
        err = bwq_enqueue(bwq, thr);
        if (err != FOS_E_SUCCESS) {
            DUAL_RET(thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
        }

        // Enqueue succeeded, we are in the clear!
        thread_detach(thr);
        thr->wq = (wait_queue_t *)bwq;
        thr->state = THREAD_STATE_WAITING;

        return FOS_E_SUCCESS;
    }

    case TERM_HCID_READ_EVENTS: {
        window_event_t *u_ev_buf = (window_event_t *)arg0;
        const size_t num_buf_cells = (size_t)arg1;
        size_t *u_cells_readden = (size_t *)arg2;

        // If we are requesting a non-zero number of requests,
        // the buffer must be non-null.
        if (num_buf_cells > 0 && !u_ev_buf) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // NOTE: The user is allowed to request 0 events. This is a 
        // non-blocking way of being able to see if there are pending events or not.

        size_t events_polled = 0; // how many cells we end up filling in the `events` buffer.
        window_event_t events[TERM_WIN_HS_MAX_EVS_PER_READ];

        if (fq_is_empty(hs_t->win_t->event_queue)) {
            // If the window is active and the event queue is empty, we just return empty!
            if (hs_t->win_t->super.is_active) {
                DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
            } 

            // If the window is inactive and there are no more events which were explicitly
            // forwarded to the terminal window, no big deal!
            // We'll allow the user to inifinitely read deregistered events!

            if (num_buf_cells > 0) {
                events[events_polled++] = (window_event_t) {
                    .event_code = WINEC_DEREGISTERED
                };
            }
        } else {
            // If the event queue is populated, we can always read from it!
            for (events_polled = 0; 
                    !fq_is_empty(hs_t->win_t->event_queue) && 
                    events_polled < MIN(num_buf_cells, TERM_WIN_HS_MAX_EVS_PER_READ);
                    events_polled++) {
                fq_poll(hs_t->win_t->event_queue, events + events_polled);
            }
        }

        // Ok, finally, now we do the copy!
        if (events_polled > 0) {
            err = mem_cpy_to_user(thr->proc->pd, u_ev_buf, events, 
                    sizeof(window_event_t) * events_polled, NULL);
            DUAL_RET_COND(err != FOS_E_SUCCESS, thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
        }

        if (u_cells_readden) {
            err = mem_cpy_to_user(thr->proc->pd, u_cells_readden, &events_polled, sizeof(size_t), NULL);
            DUAL_RET_COND(err != FOS_E_SUCCESS, thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
        }
        
        // Success!
        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

/*
 * Graphics Plugin Stuff.
 */

static fernos_error_t plg_gfx_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_gfx_cmd(plugin_t *plg, plugin_cmd_id_t cmd,
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static fernos_error_t plg_gfx_tick(plugin_t *plg);

static const plugin_impl_t PLUGIN_GFX_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = plg_gfx_kernel_cmd,
    .plg_cmd = plg_gfx_cmd,
    .plg_tick = plg_gfx_tick,
    .plg_on_fork_proc = NULL,
    .plg_on_reset_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_plugin_gfx(kernel_state_t *ks, window_t *root_window) {
    fernos_error_t err;

    plugin_gfx_t *plg_gfx = al_malloc(ks->al, sizeof(plugin_gfx_t));
    if (!plg_gfx) {
        return NULL;
    }

    // We actually allow the `root_window` resize to fail.
    // As long as the window is still active, we are good!
    err = win_resize(root_window, FERNOS_GFX_WIDTH, FERNOS_GFX_HEIGHT);

    if (err != FOS_E_INACTIVE) {
        err = win_fwd_event(root_window, (window_event_t) {
            .event_code = WINEC_UNHIDDEN,
        });
    }

    if (err != FOS_E_INACTIVE) {
        err = win_fwd_event(root_window, (window_event_t) {
            .event_code = WINEC_FOCUSED,
        });
    }

    if (err == FOS_E_INACTIVE) {
        al_free(ks->al, plg_gfx);
        return NULL;
    }

    // Success!
    init_base_plugin((plugin_t *)plg_gfx, &PLUGIN_GFX_IMPL, ks);
    *(window_t **)&(plg_gfx->root_window) = root_window;

    return (plugin_t *)plg_gfx;
}

static fernos_error_t plg_gfx_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;

    switch (kcmd) {

    case PLG_GFX_KCID_KEY_EVENT: {
        const scs1_code_t sc = (scs1_code_t)arg0;

        err = win_fwd_event(plg_gfx->root_window, (window_event_t) {
            .event_code = WINEC_KEY_INPUT,
            .d.key_code = sc
        });

        if (err == FOS_E_INACTIVE) {
            return FOS_E_INACTIVE;
        }

        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_SUCCESS;
    }

    }
}

static fernos_error_t plg_gfx_cmd(plugin_t *plg, plugin_cmd_id_t cmd,
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;
    kernel_state_t *ks = plg->ks;
    thread_t *curr_thr = (thread_t *)(ks->schedule.head);

    switch (cmd) {

    /*
     * Add a dummy window to the current root window!
     * Propagates errors to user thread.
     *
     * Really meant for debug of the root window.
     */
    case PLG_GFX_PCID_NEW_DUMMY: {
        window_t *dummy = new_window_dummy(ks->al);    
        if (!dummy) {
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        err = win_register_child(plg_gfx->root_window, dummy);
        if (err == FOS_E_INACTIVE) {
            return FOS_E_INACTIVE; // Major kernel error if root window goes 
                                   // inactive. (Don't even worry about cleanup)
        }

        if (err != FOS_E_SUCCESS) { // Non-fatal error we clean up dummy.
            delete_window(dummy); 
            DUAL_RET(curr_thr, err, FOS_E_SUCCESS);
        }

        // dummy windows are self managed.
        // The user doesn't get a reference back.

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    /*
     * Create a new terminal window!
     */
    case PLG_GFX_PCID_NEW_TERM: {
        handle_t *u_handle = (handle_t *)arg0;
        gfx_term_buffer_attrs_t *u_attrs = (gfx_term_buffer_attrs_t *)arg1;

        if (!u_handle) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // We gotta create the window, then register it? You are correct, that is what we need
        // to do!

        const handle_t NULL_HANDLE = idtb_null_id(curr_thr->proc->handle_table);

        err = FOS_E_SUCCESS;
        handle_t h = NULL_HANDLE;
        handle_terminal_state_t *hs_t = NULL;
        window_terminal_t *win_t = NULL;
        gfx_term_buffer_attrs_t attrs;

        // First let's pop a handle.
        if (err == FOS_E_SUCCESS) {
            h = idtb_pop_id(curr_thr->proc->handle_table);
            if (h == NULL_HANDLE) {
                err = FOS_E_NO_MEM;
            }
        }

        // Next let's leave space for the handle state.
        if (err == FOS_E_SUCCESS) {
            hs_t = al_malloc(plg_gfx->super.ks->al, sizeof(handle_terminal_state_t));
            if (!hs_t) {
                err = FOS_E_NO_MEM;
            }
        }

        // Next get the attributes for the terminal window we are going to create.
        if (err == FOS_E_SUCCESS) {
            if (u_attrs) {
                err = mem_cpy_from_user(&attrs, curr_thr->proc->pd, u_attrs, sizeof(gfx_term_buffer_attrs_t), NULL);
            } else {
                // Choose a default if no attributes are given.
                attrs = (gfx_term_buffer_attrs_t) {
                    .fmi = ASCII_MONO_8X16_FMI,
                    .palette = *BASIC_ANSI_PALETTE,
                    .w_scale = 1, .h_scale = 1
                };
            }
        }

        // Next let's create the terminal window
        if (err == FOS_E_SUCCESS) {
            win_t = new_window_terminal(plg->ks->al, 20, 40, &attrs, &(plg_gfx->super.ks->schedule)); 
            if (!win_t) {
                err = FOS_E_NO_MEM;
            }
        }

        // Attempt to register the window.
        if (err == FOS_E_SUCCESS) {
            err = win_register_child(plg_gfx->root_window, (window_t *)win_t);
        }

        // Finally copy out the handle to user space.
        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(curr_thr->proc->pd, u_handle, &h, sizeof(handle_t), NULL);
        }

        // I think that's it? clean up step plz!
        if (err != FOS_E_SUCCESS) {
            win_deregister((window_t *)win_t);
            delete_window((window_t *)win_t);
            al_free(plg_gfx->super.ks->al, hs_t);
            idtb_push_id(curr_thr->proc->handle_table, h);

            DUAL_RET(curr_thr, err, FOS_E_SUCCESS);
        }

        // Success!
        init_base_handle((handle_state_t *)hs_t, &HS_TERM_IMPL, plg_gfx->super.ks, curr_thr->proc, h, true);
        *(window_terminal_t **)&(hs_t->win_t) = win_t;
        win_t->references = 1; // Reference by just this handle we have returned!
        idtb_set(curr_thr->proc->handle_table, h, hs_t);

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

static fernos_error_t plg_gfx_tick(plugin_t *plg) {
    fernos_error_t err;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;

    // Here we send a tick to the root window.
    // Then render it.

    err = win_fwd_event(plg_gfx->root_window, (window_event_t) {
        .event_code = WINEC_TICK
    });
    if (err == FOS_E_INACTIVE) {
        return FOS_E_INACTIVE;
    }

    win_render(plg_gfx->root_window);
    gfx_to_screen(SCREEN, plg_gfx->root_window->buf);

    return FOS_E_SUCCESS;
}
