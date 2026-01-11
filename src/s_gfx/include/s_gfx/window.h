
#pragma once

#include "s_mem/allocator.h"
#include "s_gfx/gfx.h"
#include "s_util/ps2_scancodes.h"

/**
 * Different abstract states a window can take on.
 */
typedef uint32_t window_state_t;

/**
 * The window IS visible AND IS focused!
 * (Being "focused" doesn't really have an exact definition. Abstractly, the window is accepting
 * input at this time)
 */
#define WINSTATE_FOCUSED    (0U)

/**
 * The window is currently visible, but NOT focused!
 */
#define WINSTATE_VISIBLE    (1U)

/**
 * The window was hidden somehow. It is not currently visible, but may become visible in the future.
 */
#define WINSTATE_INVISIBLE  (2U)

/**
 * The window was closed, meaning it will never become visible again.
 * The window should no longer be displayed.
 *
 * NOTE: This is intended to be a  FINAL STATE!
 */
#define WINSTATE_CLOSED     (3U)

/**
 * The window encountered a fatal error during it's lifetime.
 * The window should no longer be displayed.
 *
 * NOTE: This is intended to be a  FINAL STATE!
 */
#define WINSTATE_ERRORED_OUT (4U)

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

    /**
     * Current window state.
     */
    window_state_t state;

    /**
     * If this window enters an error state, this field will hold why.
     */
    fernos_error_t error;
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
     * The below endpoints with prefix `window_on` are all event handlers handlers.
     *
     * ALL ARE OPTIONAL!
     *
     * If an event handler returns an error, the window will go into error state which prevents
     * further use of the window.
     *
     * Upon going into error state, `window_on_state_change` will be called one final time!
     * (Signifying a switch to the error state). 
     */

    /**
     * The window state has been changed.
     *
     * Use `w->state` to access the new state. `old_state` is the state changed from.
     */
    fernos_error_t (*window_on_state_change)(window_t *w, window_state_t old_state);

    /**
     * The window has be resized, Use `window->buf` to see new dimmensions.
     */
    fernos_error_t (*window_on_resize)(window_t *w);

    /**
     * The window has received key input.
     */
    fernos_error_t (*window_on_key_input)(window_t *w, scs1_code_t key_code);
};

/**
 * Initialize the window base fields.
 *
 * NOTE: Window starts in the INVISIBLE state!
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

/*
 * NOTE: The below functions all return FOS_E_NOT_PERMITTED when used on a closed or
 * error'd out window!
 */

/**
 * This changes the state field of the window, then afterwards call the on_state_change 
 * handler.
 */
fernos_error_t window_change_state(window_t *w, window_state_t new_state);

/**
 * This resizes the window's internal graphics buffer, then afterwards calls the on_resize
 * handler.
 */
fernos_error_t window_resize(window_t *w, uint16_t width, uint16_t height);

/**
 * Basically a wrapper around the on_key_input handler.
 */
fernos_error_t window_forward_key_input(window_t *w, scs1_code_t key_code);

