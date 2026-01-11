
#pragma once

#include "s_mem/allocator.h"
#include "s_gfx/gfx.h"

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
    allocator_t * const al;

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
     * Window implementation.
     */
    const window_impl_t * const impl;
};

struct _window_impl_t {
    uint32_t nop;
};
