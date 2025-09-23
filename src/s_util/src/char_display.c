
#include "s_util/char_display.h"

void init_char_display_base(char_display_t *base, const char_display_impl_t *impl, size_t rows, size_t cols,
        char_display_style_t default_style) {
    *(const char_display_impl_t **)&(base->impl) = impl;

    *(size_t *)&(base->rows) = rows;
    *(size_t *)&(base->cols) = cols;

    *(char_display_style_t *)&(base->default_style) = default_style;
}

static void cd_flip_cursor(char_display_t *cd) {
    char_display_style_t style;
    char c; 

    cd_get_c(cd, cd->cursor_row, cd->cursor_col, &style, &c);
    cd->impl->cd_set_c(cd, cd->cursor_row, cd->cursor_col, cds_flip(style), c);
}

/**
 * Bring the cursor up `n` lines, scrolling if needed.
 * (Cursor moves to the beginning of the new line)
 */
static void cd_cursor_prev_line(char_display_t *cd, size_t n) {
    if (cd->cursor_row < n) {
        cd->impl->cd_scroll_down(cd, n - cd->cursor_row);
        cd->cursor_row = 0; 
    } else {
        cd->cursor_row -= n;
    }

    cd->cursor_col = 0;
}

/**
 * Bring the cursor down `n` lines, scrolling if needed.
 * (Cursor moves to the beginning of the new line)
 */
static void cd_cursor_next_line(char_display_t *cd, size_t n) {
    size_t dest_row = cd->cursor_row + n;
    if (dest_row >= cd->rows) {
        cd->impl->cd_scroll_up(cd, dest_row - (cd->rows - 1));
        cd->cursor_row = cd->rows - 1;
    } else {
        cd->cursor_row = dest_row;
    }

    cd->cursor_col = 0;
}

/**
 * Advnace the cursor one spot to the right, advancing if needed.
 */
static void cd_cursor_advance(char_display_t *cd) {
    if (cd->cursor_col == cd->cols - 1) {
        cd_cursor_next_line(cd, 1);
    } else {
        cd->cursor_col++;
    }
}

void cd_put_c(char_display_t *cd, char c) {
    cd_flip_cursor(cd);

    switch (c) {
    case '\n':
        cd_cursor_next_line(cd, 1);
        break;
    case '\r':
        cd->cursor_col = 0;
        break;
    default: {
        char c_to_print = 32 <= c && c < 127 ? c : ' ';
        cd->impl->cd_set_c(cd, cd->cursor_row, cd->cursor_col, cd->curr_style, c_to_print);
        cd_cursor_advance(cd);        
        break;
    }

    }

    cd_flip_cursor(cd);
}

/**
 * Iterpret an ANSI escape sequence. This assume that `*s` points to the
 * first character after `\x1B`.
 *
 * Returns the number of characters consumed.
 */
static size_t cd_interp_esc_seq(char_display_t *cd, const char *s) {
    const char *i = s;
    char c;

    if (*i != '[') {
        return 0;   
    }

    uint8_t arg0 = 0;
    uint8_t arg1 = 0;

    // We allow the parsing of just 2 arguments for now.

    while ((c = *(++i)) && ('0' <= c && c <= '9')) {
        arg0 *= 10;
        arg0 += c - '0';
    }

    if (c == ';') {
        while ((c = *(++i)) && ('0' <= c && c <= '9')) {
            arg1 *= 10;
            arg1 += c - '0';
        }
    }

    // At this point, `c` will hold the command identifying character.
    // We'll just consume it here now instead of later.
    //
    // (NOTE: We are consuming it even if it's an unknown command)
    i++;

    switch (c) {

    /*
     * Simple movement.
     */

    case 'A': // Cursor Up.
        if (cd->cursor_row < arg0) {
            cd->cursor_row = 0;
        } else {
            cd->cursor_row -= arg0;
        }
        break;
    case 'B': // Cursor Down.
        if (cd->cursor_row + arg0 >= cd->rows) {
            cd->cursor_row = cd->rows - 1;
        } else {
            cd->cursor_row += arg0;
        }
        break;
    case 'C': // Cursor Right.
        if (cd->cursor_col + arg0 >= cd->cols) {
            cd->cursor_col = cd->cols - 1;
        } else {
            cd->cursor_col += arg0;
        }
        break;
    case 'D': // Cursor Left.
        if (cd->cursor_col < arg0) {
            cd->cursor_col = 0;
        } else {
            cd->cursor_col -= arg0;
        }
        break;

    /*
     * Next/Previous Line.
     */

    case 'E': // Cursor Next Line.
        cd_cursor_next_line(cd, arg0);
        break;

    case 'F': // Cursor Previous Line.
        cd_cursor_prev_line(cd, arg0);
        break;

    case 'J': // Cursor Clear Display.
        if (arg0 == 2) {
            cd->cursor_row = 0;
            cd->cursor_col = 0;
            
            cd->impl->cd_set_grid(cd, cd->default_style, ' ');
        }
        break;

    case 'K': // Clear line options
        break;

    case 'G': // Set Cursor col
        break;

    case 'H': // Set cursor position
        break;

    case 'S': // Scroll up
        break;

    case 'T': // Scroll Down.
        break;

    case 'm': // Colors!!!
        break;
    default:
        break;
    }

    return i - s; 
}

void cd_put_s(char_display_t *cd, const char *s) {
    const char *iter = s;
    char c;

    while ((c = *iter)) {

        iter++;
    }
}
