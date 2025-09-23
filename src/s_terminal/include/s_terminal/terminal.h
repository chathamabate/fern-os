
#pragma once

#include "s_terminal/char_display.h"
#include "s_mem/allocator.h"

typedef struct _terminal_t terminal_t;
typedef struct _terminal_impl_t terminal_impl_t;

struct _terminal_impl_t {
    // Well, I mean, what should happen here... that's the real question I guess...
    // Maybe we could make some key_listener interface first before making this???
    int dummy;
};

/**
 * A terminal is a wrapper around a character display which adds some
 * conveinence behaviors. 
 *
 * For example, working with a cursor; scrolling; history; I/O... and more.
 */
struct _terminal_t {
    allocator_t * const al;

    /**
     * The underlying display.
     *
     * NOTE: As of now, this is owned by the terminal object. When the terminal is deleted,
     * so is the character display.
     */
    char_display_t * const cd;

    /**
     * A terminal may have more rows than the character display can display at once.
     *
     * This is the total number of rows held in memory by this terminal. 
     * This allows for scrolling through history.
     */
    const size_t all_rows;

    /**
     * An array of character pairs with size (`all_rows` * `cd->cols`).
     */
    char_display_pair_t * const cyclic_grid;

    /**
     * To make things fast, the grid is cyclic.
     *
     * The oldest row in history will be stored at index `cg_origin_row` * `cd->cols`.
     * 
     * If a new row is needed, instead of actually shifting around character pairs in 
     * memory, `cg_origin_row` will just be incremented (looping back to 0 if needed).
     * The old value of `cg_origin_row` will be used for the newest row index!
     */
    size_t cg_origin_row;

    /**
     * The "Window" is the area of the grid which is actually currently being displayed
     * in the character display. So, the 0th row in the character display will retrieved
     * from the `window_row`'th row in the `cyclic_grid`.
     */
    size_t window_row;

    /**
     * The offset of the cursor relative to the `window_row`.
     *
     * This value will always be less than `cd->rows`.
     */
    size_t cursor_row_offset;

    /**
     * The column of the cursor.
     */
    size_t cursor_col;

    /**
     * If the cursor is visible.
     */
    bool cursor_visible;

    /**
     * When an ANSI Reset code is processed, this is the style returned to.
     */
    char_display_style_t default_style;

    /**
     * The current style used for printing text.
     */
    char_display_style_t curr_style;
};

/**
 * Create a new terminal.
 *
 * `num_rows` is the total number of rows stored in memory by the terminal.
 *
 * Returns NULL if something fails.
 */
terminal_t *new_terminal(allocator_t *al, char_display_t *cd, size_t num_rows, char_display_style_t default_style);

/**
 * Delete a terminal
 */
void delete_terminal(terminal_t *t);

/**
 * Write a character to the terminal.
 *
 * Control Characters Supported:
 * '\n' - Move cursor to the beginning of the next line. (Scrolling if needed)
 * '\r` - Move cursor to the beginning of the current line.
 *
 * Undefined behavior if `c` is not a ASCII printable character or one of the listed control 
 * sequences.
 */
void t_put_c(terminal_t *t, char c);

/**
 * Write a string to the terminal.
 *
 * Control Characters Supported:
 * '\n' - Move cursor to the beginning of the next line. (Scrolling if needed)
 * '\r` - Move cursor to the beginning of the current line.
 *
 * ANSI Control Sequences Supported: See "s_util/ansi.h"
 *
 * Undefined behavior if a non-supported control character or sequence is encountered in `s`.
 */
void t_put_s(terminal_t *t, const char *s);

