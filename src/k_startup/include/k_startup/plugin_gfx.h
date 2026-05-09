
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

#include "s_data/map.h"
#include "s_data/fixed_queue.h"
#include "s_gfx/window.h"
#include "s_gfx/mono_fonts.h"

/**
 * I realized that there will be overlap for terminal windows and simple
 * graphics buffer windows in terms of how events are handled.
 *
 * So here is a nice superclass to help with this!
 */
typedef struct _window_gfx_base_t window_gfx_base_t;
struct _window_gfx_base_t {
    window_t super;

    /**
     * The window structure as a whole MUST be allocated with this allocator!
     */
    allocator_t * const al;

    /**
     * Number of open handles which reference this window.
     */
    size_t references;

    /**
     * Events forwarded to the terminal window will be here.
     */
    fixed_queue_t * const eq;

    /**
     * Threads waiting for the event queue to be non-empty.
     */
    basic_wait_queue_t * const bwq;

    /**
     * Woken threads are scheduled here.
     */
    ring_t * const schedule;
};

/**
 * Remember that if this succeeds, `buf` will now be owned by `win`!
 * `win` MUST be allocated by `al`.
 *
 * References will be set as 1.
 */
fernos_error_t init_window_gfx_base(window_gfx_base_t *win, gfx_buffer_t *buf, const window_impl_t *impl, allocator_t *al, ring_t *sch);

/**
 * GFX Base windows are promised to be dynamically allocated.
 * This will free `win` with its internal allocator.
 * Make sure subclasses cleanup their private fields before calling this!
 */
void delete_window_gfx_base(window_gfx_base_t *win);

typedef struct _window_terminal_t window_terminal_t;
typedef struct _handle_terminal_state_t handle_terminal_state_t;

/**
 * A Terminal Window is a special window accessible via handle which just displays a grid of text.
 */
struct _window_terminal_t {
    window_gfx_base_t super;

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

typedef struct _window_gfx_t window_gfx_t;
typedef struct _handle_gfx_state_t handle_gfx_state_t;

struct _window_gfx_t {
    window_gfx_base_t super;

    /**
     * This will require a kernel state to map/unmap the graphics banks.
     */
    kernel_state_t * const ks;

    /**
     * The buffer pointer in the window base class will point here!
     * The actual buffer in here will swap between banks[0] and banks[1].
     */
    gfx_buffer_t static_buffer;

    /**
     * When a gfx window is created, each `bank` will be allocated in the shared memory area
     * with a size of SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(gfx_color_t).
     */
    gfx_color_t * const banks[2];
};

struct _handle_gfx_state_t {
    handle_state_t super;

    window_gfx_t * const win_g;
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
