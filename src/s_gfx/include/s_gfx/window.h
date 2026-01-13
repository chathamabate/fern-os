
#pragma once

#include "s_mem/allocator.h"
#include "s_gfx/gfx.h"
#include "s_util/ps2_scancodes.h"

typedef struct _window_t window_t;
typedef struct _window_impl_t window_impl_t;

struct _window_t {
    /**
     * Window implementation.
     */
    const window_impl_t * const impl;

    /**
     * All windows require a graphics buffer.
     *
     * This buffer is free to be written to directly by whomever, whenever.
     * Just be careful, as it's also free to be read from whenever.
     * Screen tearing may occur depending on how this window is managed and output to the screen.
     */
    gfx_buffer_t * const buf;
};

struct _window_impl_t {
    /**
     * Delete the window!
     *
     * NOTE: BE VERY SURE TO CALL `deinit_window` on `w` in here to delete the dynamic parts of the 
     * parent class!
     */
    void (*delete_window)(window_t *w);

    /*
     * Below endpoints are ALL OPTIONAL!
     */

    /**
     * When a window's gfx buffer is resized successfully, this is called afterwards.
     * An error returned from this function other than FOS_E_SUCCESS is seen as fatal for the 
     * window.
     */
    fernos_error_t (*win_on_resize)(window_t *w);

    fernos_error_t (*win_forward_key_input)(window_t *w, scs1_code_t key_code);
    fernos_error_t (*win_tick)(window_t *w);
};

/**
 * Initialize the window base fields.
 *
 * NOTE: Window starts in the INVISIBLE state!
 *
 * `buf` becomes OWNED by `w`!
 */
void init_window_base(window_t *w, gfx_buffer_t *buf, const window_impl_t *impl);

/**
 * This deletes the dynamic parts of the window base fields.
 *
 * (Remember that `window_t` is an abstract base class meant to be embedded into concrete
 * implementations)
 */
void deinit_window_base(window_t *w);

/**
 * Delete a window.
 */
static inline void delete_window(window_t *w) {
    if (w) {
        w->impl->delete_window(w);
    }
}


/**
 * This resizes the window's internal graphics buffer, then afterwards calls the on_resize
 * handler.
 *
 * FOS_E_SUCCESS if the resize succeeded.
 * FOS_E_NO_MEM if there was a failure with this initial resize of the gfx buffer.
 * FOS_E_FATAL if the initial resize succeeded, but the on_resize handler failed.
 * The window should be deleted and no longer used in this case.
 */
fernos_error_t win_resize(window_t *w, uint16_t width, uint16_t height);

/**
 * Forward a keycode to the window.
 *
 * FOS_E_SUCCESS if the key code was successfully forwarded.
 * FOS_E_FATAL if the key press failed, and the window is no longer useable.
 * Other errors mean the key press failed, but the window is still useable.
 */
static inline fernos_error_t win_forward_key_input(window_t *w, scs1_code_t key_code) {
    if (w->impl->win_forward_key_input) {
        return w->impl->win_forward_key_input(w, key_code);
    }
    return FOS_E_SUCCESS;
}

/**
 * Tell the window a time step has passed, and that it can update its state/graphics buffer.
 *
 * FOS_E_SUCCESS if the tick succeeded.
 * FOS_E_FATAL if the tick failed and the window is no longer useable.
 * Other errors mean the tick failed, but the window is still useable.
 */
static inline fernos_error_t win_tick(window_t *w) {
    if (w->impl->win_tick) {
        return w->impl->win_tick(w);
    }
    return FOS_E_SUCCESS;
}

