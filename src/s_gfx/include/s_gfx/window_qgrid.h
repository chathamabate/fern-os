
#pragma once

#include "s_gfx/window.h"

/**
 * No tile will ever occupy a space smaller than MIN_TILE_DIM x MIN_TILE_DIM.
 */
#define WIN_QGRID_MIN_TILE_DIM (100U)
#define WIN_QGRID_BORDER_WIDTH (2U)

/**
 * The focused tile will have an extra thinner border.
 */
#define WIN_QGRID_FOCUS_BORDER_WIDTH (1U)

/*
 * The colors used by the quad grid window.
 * (Consider having these be specifiable at runtime per window)
 */

#define WIN_QGRID_BG_COLOR               gfx_color(0, 0, 30)
#define WIN_QGRID_BORDER_COLOR           gfx_color(0, 0, 80)
#define WIN_QGRID_FOCUS_BORDER_COLOR     gfx_color(0, 40, 160)
#define WIN_QGRID_FOCUS_ALT_BORDER_COLOR gfx_color(255, 255, 255)
#define WIN_QGRID_FOCUS_MOV_BORDER_COLOR gfx_color(0, 255, 0)

/**
 * The minimum height/width of the entire qgrid window.
 *
 * DON"T CHANGE!
 */
#define WIN_QGRID_MIN_DIM ((2 * WIN_QGRID_MIN_TILE_DIM) + (3 * WIN_QGRID_BORDER_WIDTH))

/**
 * A qgrid window is really a simple test composite window.
 *
 * qgrid is for Quad Grid. 
 * This window will be a 2x2 Grid of subwindows.
 *
 * The quad grid window will have 2 states:
 * 1) Grid Mode
 * 2) Single Pane Mode
 *
 * In Grid Mode, all 4 tiles will be visible. 
 * A tile being 1 cell in the 2x2 grid.
 * Each tile can either hold one subwindow or be empty.
 * There is at most 1 focused tile. If the focused tile contains a window,
 * that window is forwarded key events.
 *
 * In Single Pane Mode, one of the 4 tiles is expanded to fill the whole 
 * screen. 
 * Entering/Exiting Single Pane Mode resizes whatever window is being 
 * expanded/shrunk.
 *
 * In both modes, all subwindows are forwarded tick events.
 * The mode determines which subwindow is actually rendered.
 *
 * IMPORTANT: Quad Grid Window's assume subwindows never resize themselves!
 * If a window cannot be resized due to its min/max dimmension attributes, it will still be 
 * rendered starting at the top left corner of its tile. If the window is larger than the tile,
 * its excess is clipped. If the window is smaller than the tile, the gap will be blacked out.
 *
 * Controls:
 * <ALT> + Arrow Keys : Navigate around panes. (Works in both modes)
 * <ALT> + <SHIFT> + Arrow Keys : Switch position of panes (Works in both modes)
 * <ALT> + F : Enter/Exit Single Pane mode with whatever tile is focused.
 * <ALT> + X : Deregister the window in the focused tile.
 *
 * (When <ALT> is held, no key inputs are forwarded to subwindows)
 *
 * Finally, inactive subwindows are deregistered.
 */
typedef struct _window_qgrid_t {
    window_t super;

    /**
     * A qgrid window will always be dynamically allocated.
     */
    allocator_t * const al;

    /**
     * The tiles. 
     *
     * NULL means an empty tile!
     *
     * In single pane mode, the focused pane will have a large size where as all
     * other panes will have the normal tile size.
     * In grid mode, all tiles have the normal tiles size.
     */
    window_t *grid[2][2];

    size_t focused_row;
    size_t focused_col;

    /**
     * Are we in single pane mode?
     */
    bool single_pane_mode;

    /**
     * Is <ALT> currently pressed?
     */
    bool lalt_held, ralt_held;

    /**
     * Is <SHIFT> currently pressed?
     */
    bool lshift_held, rshift_held;
} window_qgrid_t;

/*
 * The below functions are used for getting the correct dimmensions of a tile in different 
 * contexts.
 *
 * In grid mode, all tiles have the same dimmensions. Use `win_qg_tile_width/height`
 * In single pane mode, all unfocused tiles are invisible and retain their dimmensions from
 * grid mode. The focused tile is expanded with size `win_qg_large_tile_width/height`.
 *
 * NOTE: When we are in single pane mode and switch panes, the new focused pane will be expanded
 * whereas the initial pane will be shrunk.
 */

static inline uint16_t win_qg_tile_width(window_qgrid_t *win_qg) {
    return (win_qg->super.buf->width - (WIN_QGRID_BORDER_WIDTH * 3)) / 2;
}

static inline uint16_t win_qg_tile_height(window_qgrid_t *win_qg) {
    // BORDER WIDTH AND HEIGHT ARE THE SANE! THIS IS CORRECT!
    return (win_qg->super.buf->height - (WIN_QGRID_BORDER_WIDTH * 3)) / 2;
}

static inline uint16_t win_qg_large_tile_width(window_qgrid_t *win_qg) {
    return win_qg->super.buf->width - (WIN_QGRID_BORDER_WIDTH * 2);
}

static inline uint16_t win_qg_large_tile_height(window_qgrid_t *win_qg) {
    return win_qg->super.buf->height - (WIN_QGRID_BORDER_WIDTH * 2);
}

/**
 * Create a new Quad Grid Window.
 */
window_t *new_window_qgrid(allocator_t *al, uint16_t width, uint16_t height);

static inline window_t *new_da_window_qgrid(uint16_t width, uint16_t height) {
    return new_window_qgrid(get_default_allocator(), width, height);
}
