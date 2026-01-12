
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
 * NOTE: This is intended to be a FINAL STATE!
 */
#define WINSTATE_CLOSED     (3U)

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
     *
     * NEVER EVER change this value directly, always use helpers below!
     */
    window_state_t state;

    /**
     * If the window is closed, this is the status it was closed with.
     */
    fernos_error_t close_status;
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
     * The below endpoints with prefix `win_on` are all event handlers handlers.
     *
     * ALL ARE OPTIONAL!
     *
     * NOTE: You should never call these functions directly!
     * See the below wrappers!
     *
     * NOTE: You may notice there is no specific "render" function. When you choose to write to 
     * the graphics buffer is up to you. You may do this every tick. You may only do this
     * when keys are pressed or window states are changed. The choice is yours!
     * You may even decide to write to the graphics buffer from external sources/events
     * not generic enough to be defined here. (For example, system calls)
     */

    /**
     * The window state has been changed.
     *
     * Use `w->state` to access the new state. `old_state` is the state changed from.
     */
    void (*win_on_state_change)(window_t *w, window_state_t old_state);

    /**
     * The window has been resized, Use `window->buf` to see new dimmensions.
     */
    void (*win_on_resize)(window_t *w);

    /**
     * The window has received key input.
     */
    void (*win_on_key_input)(window_t *w, scs1_code_t key_code);

    /**
     * A time step has passed.
     */
    void (*win_on_tick)(window_t *w);
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

/*
 * NOTE: The below functions all do nothing if the window is already closed!
 *
 * NOTE: When trying to propegate errors, things became confusing quickly. Especially if the user 
 * decides to call one of the below functions in one of the handlers. For example, what if the
 * user would like to change the state of a window after a specific keypress? Or maybe forward
 * a keypress to themself on focus?
 * I wanted the caller to be able to easily see if one of the below functions resulted in a 
 * change of window state. All below calls return the state of the window upon entering the
 * function. To detect a change of state, just compare the return value to the current state.
 * i.e. if `old_state != w->state` -> do some state change logic if necessary.
 */

/**
 * This changes the state field of the window, then afterwards call the on_state_change 
 * handler.
 *
 * This does nothing if `new_state` is equal to `w`'s current state.
 *
 * If `new_state` == CLOSED, this closes the window with an FOS_E_SUCCESS close status!
 */
window_state_t win_change_state(window_t *w, window_state_t new_state);

/**
 * Close the window with the given close status.
 */
window_state_t win_close(window_t *w, fernos_error_t close_status);

/**
 * This resizes the window's internal graphics buffer, then afterwards calls the on_resize
 * handler.
 */
window_state_t win_resize(window_t *w, uint16_t width, uint16_t height);

/**
 * Basically a wrapper around the on_key_input handler.
 */
window_state_t win_forward_key_input(window_t *w, scs1_code_t key_code);

/**
 * Basically a wrapper around the on_tick handler.
 */
window_state_t win_tick(window_t *w);

