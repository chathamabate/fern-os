
#pragma once

#include "s_mem/allocator.h"
#include "s_gfx/gfx.h"
#include "s_util/ps2_scancodes.h"

/**
 * Windows will support an event driven design.
 */
typedef uint32_t window_event_code_t;

/**
 * An event which can be expected to occur repeatedly while a window is active.
 */
#define WINEC_TICK (0U)

/**
 * The window buffer was resized.
 *
 * DON"T SEND MANUALLY, use `win_resize` below!
 */
#define WINEC_RESIZED (1U)

/**
 * The window has received key input.
 */
#define WINEC_KEY_INPUT (2U)

/**
 * The window was deregistered from a continer window.
 *
 * DON"T SEND MANUALLY, use `win_deregister` below!
 */
#define WINEC_DEREGISTERED (3U)

typedef struct _window_event_t {
    window_event_code_t event_code;

    union {
        /**
         * Populated when `event_code == WINEC_KEY_INPUT`.
         */
        scs1_code_t key_code;
    } d;
} window_event_t;

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
     * Is this window active? 
     *
     * For inactive windows, all operation return FOS_E_INACTIVE.
     */
    bool is_active;

    /**
     * If this window is a subwindow of some larger composite window, this points to the composite
     * window. Otherwise, this is NULL.
     */
    window_t *container;
};

struct _window_impl_t {
    /**
     * Delete the window!
     *
     * NOTE: BE VERY SURE TO CALL `deinit_window` on `w` in here to delete the dynamic parts of the 
     * parent class!
     */
    void (*delete_window)(window_t *w);

    /**
     * Handle an event!
     *
     * To signal to the user that this window should no longer be used, return FOS_E_INACTIVE.
     * The wrapper of this function will deactivate the window in this case.
     * 
     * SPECIAL EVENTS:
     *
     * WINEC_RESIZED
     * This event should only be sent to a window from inside the `win_resize` function below.
     * It is only forwarded AFTER the window's buffer has successfully been resized.
     * Returning any error code other than FOS_E_SUCCESS deactivates the window.
     *
     * WINEC_DEREGISTERED
     * This event should only be sent to a window from inside the `win_deregister` function below.
     * It is only forwarded AFTER the container window's deregister function has run AND
     * `w->container` has been set to NULL. The receiver of this event code can assume that it is
     * no longer referenced by the old container window. 
     * The return value of this handler is ignored.
     */
    fernos_error_t (*win_on_event)(window_t *w, window_event_t ev);

    /*
     * Below endpoints are ALL OPTIONAL!
     */

    /*
     * The below two endpoints have some automatic checking before being called.
     * I usually don't do this type of thing, but I am making an exception.
     */

    /**
     * When this endpoint is called, it is gauranteed `sw` is non-null
     * AND `sw->container` is NULL.
     *
     * Afterwards, the wrapper will set `sw->container` to `w`.
     */
    fernos_error_t (*win_register_child)(window_t *w, window_t *sw);

    /**
     * When this handler is called, it is gauranteed `sw` is non-null 
     * AND that `sw->container == w`.
     *
     * Afterwards, the wrapper will set `sw->container` to NULL.
     * It will finally call send a deregister event to `sw`.
     *
     * VERY IMPORTANT, this handler should remove ALL references to `sw`
     * from `w`. `sw` assumes this, and may be deleted after this handler
     * runs.
     */
    void (*win_deregister_child)(window_t *w, window_t *sw);
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
 * This resizes the window's internal graphics buffer, then afterwards sends a 
 * window resize event to `w`.
 *
 * FOS_E_INACTIVE if this window is inactive.
 * FOS_E_SUCCESS if the resize succeeded.
 * FOS_E_NO_MEM if there was a failure with this initial resize of the gfx buffer.
 */
fernos_error_t win_resize(window_t *w, uint16_t width, uint16_t height);

/**
 * Forward an event to a window!
 *
 * FOS_E_SUCCESS means the event succeeded.
 * FOS_E_INACTIVE means this window is inactive.
 * Other errors mean the event failed, but the window is still active.
 */
fernos_error_t win_fwd_event(window_t *w, window_event_t ev);

/*
 * Composite window functions.
 */

/**
 * Register a subwindow. (You decide exactly what this means)
 *
 * FOS_E_NOT_IMPLEMENTED if this window does not support subwindows.
 * FOS_E_STATE_MISMATCH if `sw` already belongs to a container or `sw` is inactive.
 * FOS_E_SUCCESS if the subwindow was successfully registered.
 * FOS_E_INACTIVE if `w` is no longer useable.
 * Other errors mean the register failed, but the window is still useable.
 *
 * VERY IMPORTANT, On Success, this automatically set's `sw->container` to `w`.
 */
fernos_error_t win_register_child(window_t *w, window_t *sw);

/**
 * Deregister `w` from it's parent container.
 *
 * In order this function:
 * 1) Calls `w->container`'s deregister function
 * 2) Sets `w->container` to NULL
 * 3) Forwards a deregistered event to `w`.
 */
void win_deregister(window_t *w);

