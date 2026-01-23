
#pragma once

#include "k_startup/plugin.h"

#include "s_gfx/window.h"

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
 *
 * Returns NULL on error.
 */
plugin_t *new_plugin_gfx(kernel_state_t *ks, window_t *root_window);
