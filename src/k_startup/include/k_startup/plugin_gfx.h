
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

#include "s_data/map.h"
#include "s_gfx/window.h"
#include "s_gfx/mono_fonts.h"

typedef struct _plugin_gfx_t plugin_gfx_t;

typedef struct _window_terminal_t window_terminal_t;
typedef struct _handle_terminal_state_t handle_terminal_state_t;

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
 * A Terminal Window is a special window accessible via handle which just displays a grid of text.
 */
struct _window_terminal_t {
    window_t super;

    /**
     * This will map `handle_terminal_state_t *` -> `uint32_t`.
     * The number value will not be used, only the keys will matter.
     *
     * This set will contain ever handle state which references this terminal.
     * (We need to be able to notifiy handles when events arrive!)
     */
    map_t * const handle_set;
    
    /**
     * How the terminal buffer will be rendered on screen.
     */
    gfx_term_buffer_attrs_t tb_attrs;

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
 * Create a new terminal window!
 *
 * The window will have dimmensions `rows` X `cols` IN CHARACTER CELLS! 
 */
window_terminal_t *new_window_terminal(allocator_t *al, uint16_t rows, uint16_t cols, 
        const gfx_term_buffer_attrs_t *attrs);

/**
 * This is a handle state which references a Terminal Window!
 *
 * It is notified when pending events exist in the terminal window's buffer.
 * It has the ability to fetch events which have occured and output text to the Terminal
 * Window!
 */
struct _handle_terminal_state_t {
    handle_state_t super;

    // TODO - FILL ME IN!
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
