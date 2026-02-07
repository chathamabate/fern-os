
#pragma once

#include "s_gfx/window.h"

/**
 * The side length of a single square cell in the dummy window.
 */
#define WIN_DUMMY_CELL_DIM (20U)

#define WIN_DUMMY_ROWS (10U)
#define WIN_DUMMY_COLS (10U)

#define WIN_DUMMY_GRID_WIDTH  (WIN_DUMMY_COLS * WIN_DUMMY_CELL_DIM)
#define WIN_DUMMY_GRID_HEIGHT (WIN_DUMMY_ROWS * WIN_DUMMY_CELL_DIM)

typedef uint8_t window_dummy_direction_t;

#define WIN_DUMMY_EAST  (0U)
#define WIN_DUMMY_NORTH (1U)
#define WIN_DUMMY_WEST  (2U)
#define WIN_DUMMY_SOUTH (3U)

/**
 * A Dummy Window is just an example of a self managed window.
 * It's purpose is for testing.
 *
 * On render, it displays a centered grid with square cells.
 *
 * The cursor tile will be filled with a unique color.
 * When the window is focused, the cursor will be bright, whereas
 * when the window is unfocused, the cursor will be dull.
 *
 * On key press, arrow keys can be used to change the direction of the cursor.
 *
 * The window can be resized, but will have a minimum size of 
 * `ROWS * CELL_DIM` X `COLS * CELL_DIM`.
 *
 * As the window is self-managed, deregistering this window will call the 
 * destructor!
 */
typedef struct _window_dummy_t {
    window_t super;

    allocator_t * const al;

    /**
     * The row of the selected cell in the grid.
     */
    uint16_t cursor_row;

    /**
     * The column of the selected cell in the grid.
     */
    uint16_t cursor_col;
} window_dummy_t;

/**
 * Create a new dummy window!
 */
window_t *new_window_dummy(allocator_t *al);

static inline window_t *new_da_window_dummy(void) {
    return new_window_dummy(get_default_allocator());
}
