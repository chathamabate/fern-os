
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

#include "s_gfx/window.h"
#include "s_gfx/mono_fonts.h"

typedef struct _plugin_gfx_t plugin_gfx_t;

typedef struct _window_char_display_attrs_t window_char_display_attrs_t;
typedef struct _window_char_display_t window_char_display_t;
typedef struct _handle_cd_state_t handle_cd_state_t;

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
 * The attributes of the character display.
 * (Intended for cosmetic customization)
 *
 * This structure shouldn't hold any pointers to allow for easy
 * copying from userspace to kernel space.
 */
struct _window_char_display_attrs_t {
    /**
     * Scale of the text to render.
     */
    uint8_t w_scale, h_scale;

    /**
     * Palette to use for text.
     */
    gfx_ansi_palette_t palette;
};

/**
 * A character display is a window which display only a grid of monospace text.
 * It will display as many font
 */
struct _window_char_display_t {
    window_t super;


};

typedef struct _handle_cd_state_t {
    handle_state_t super;

    // Ok, and what does a handle state actually hold???
    // A reference counted window???
} handle_cd_state_t;

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
