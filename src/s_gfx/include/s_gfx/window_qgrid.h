
#pragma once

#include "s_gfx/window.h"

/**
 * No tile will ever occupy a space smaller than MIN_TILE_DIM x MIN_TILE_DIM.
 */
#define WIN_QGRID_MIN_TILE_DIM (100U)
#define WIN_QGRID_BORDER_WIDTH (10U)

/**
 * The focused tile will have an extra thinner border.
 */
#define WIN_QGRID_FOCUS_BORDER_WIDTH (3U)

/**
 * The minimum height/width of the entire qgrid window.
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
 * All subwindows are forwarded tick events.
 * There is at most 1 focused tile. If the focused tile contains a window,
 * that window is forwarded key events.
 *
 * In Single Pane Mode, one of the 4 tiles is expanded to fill the whole 
 * screen. Only that tile will receive tick events.
 * Entering/Exiting Single Pane Mode resizes whatever window is being 
 * expanded/shrunk.
 *
 * IMPORTANT: Quad Grid Window's assume subwindows never resize themselves!
 * If a window cannot be resized due to its min/max dimmension attributes, it will still be 
 * rendered starting at the top left corner of its tile. If the window is larger than the tile,
 * its excess is clipped. If the window is smaller than the tile, the gap will be blacked out.
 *
 * Controls:
 * <CNTL> + Arrow Keys : Navigate around panes. (Only in Grid Mode)
 * <CNTL> + F : Enter/Exit Single Pane mode with whatever tile is focused.
 * <CNTL> + X : Deregister the window in the focused tile.
 *
 * (When <CNTL> is held, no key inputs are forwarded to subwindows)
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
     */
    window_t *grid[2][2];

    size_t focused_row;
    size_t focused_col;

    /**
     * Are we in single pane mode?
     */
    bool single_pane_mode;

    /**
     * Is <CNTL> currently pressed?
     */
    bool cntl_held;

    /**
     * Is the qgrid window itself focused?
     */
    bool focused;
} window_qgrid_t;

/**
 * Create a new Quad Grid Window.
 */
window_t *new_window_qgrid(allocator_t *al, uint16_t width, uint16_t height);
