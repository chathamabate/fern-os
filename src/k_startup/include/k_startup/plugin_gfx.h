
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

#include "s_data/map.h"
#include "s_data/fixed_queue.h"
#include "s_gfx/window.h"
#include "s_gfx/mono_fonts.h"

typedef struct _window_terminal_t window_terminal_t;
typedef struct _handle_terminal_state_t handle_terminal_state_t;

/**
 * A Terminal Window is a special window accessible via handle which just displays a grid of text.
 */
struct _window_terminal_t {
    window_t super;

    /**
     * Terminal windows will always be allocated dynamically.
     */
    allocator_t * const al;

    /**
     * Number of open handles which reference this Terminal Window.
     */
    size_t references;

    /**
     * How the terminal buffer will be rendered on screen.
     */
    const gfx_term_buffer_attrs_t tb_attrs;

    /**
     * The value of the terminal buffer which is currently rendered in super.buf.
     */
    term_buffer_t * const visible_tb;

    /**
     * When a terminal window is resized, it's buffer pixels are in an unknown state.
     * When this is true, assume none of `visible_tb` is actually rendered yet.
     */
    bool dirty_buffer;

    /**
     * The true value of the terminal buffer which should be rendered out next frame!
     */
    term_buffer_t * const true_tb;

    /**
     * Is this window focused?
     */
    bool focused;

    /**
     * The tick number. (used for cursor flashing)
     */
    uint32_t tick;

    /**
     * Events forwarded to the terminal window will be 
     */
    fixed_queue_t * const event_queue;

    /**
     * Threads waiting for the event queue to be non-empty.
     */
    basic_wait_queue_t * const wq;

    /**
     * Woken threads are scheduled here.
     */
    ring_t * const schedule;
};

/**
 * This is a handle state which references a Terminal Window!
 *
 * It is notified when pending events exist in the terminal window's buffer.
 * It has the ability to fetch events which have occured and output text to the Terminal
 * Window!
 */
struct _handle_terminal_state_t {
    handle_state_t super;

    window_terminal_t * const win_t;
};

typedef struct _plugin_gfx_t plugin_gfx_t;

/**
 * The graphics plugin will manage the FernOS Desktop environment.
 *
 * It will also give userspace hooks for creating desktop windows!
 */
struct _plugin_gfx_t {
    plugin_t super;

    window_t * const root_window;
};

/**
 * Create a new graphics plugin!
 *
 * This graphics plugin will manage the given `root_window`!
 *
 * Upon creating the plugin, `root_window` will be forwarded a resize event with the screen
 * dimmensions.
 * It will then be forwarded a focus event!
 *
 * Returns NULL on error.
 */
plugin_t *new_plugin_gfx(kernel_state_t *ks, window_t *root_window);
