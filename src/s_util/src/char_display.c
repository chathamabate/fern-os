
#include "s_util/char_display.h"

char_display_style_t cds_with_ansi_style_code(char_display_style_t cds, uint8_t ansi_style_code) {
    switch (ansi_style_code) {
    default:
    case 0: // Reset
        return char_display_style(CDC_WHITE, CDC_BLACK);
    case 1: // BOLD
        return cds | CDS_BOLD;
    case 3: // ITALIC
        return cds | CDS_ITALIC;
    case 30: // BLACK_FG
        return cds_with_fg_color(cds, CDC_BLACK);
    case 31: // BLUE_FG
        return cds_with_fg_color(cds, CDC_BLUE);
    case 32: // GREEN_FG
        return cds_with_fg_color(cds, CDC_GREEN);
    case 33: // CYAN_FG
        return cds_with_fg_color(cds, CDC_CYAN);
    case 34: // RED_FG
        return cds_with_fg_color(cds, CDC_RED);
    case 35: // MAGENTA_FG
        return cds_with_fg_color(cds, CDC_MAGENTA);
    case 36: // BROWN_FG
        return cds_with_fg_color(cds, CDC_BROWN);
    case 37: // LIGHT_GREY_FG
        return cds_with_fg_color(cds, CDC_LIGHT_GREY);
    case 90: // BRIGHT_BLACK_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_BLACK);
    case 91: // BRIGHT_BLUE_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_BLUE);
    case 92: // BRIGHT_GREEN_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_GREEN);
    case 93: // BRIGHT_CYAN_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_CYAN);
    case 94: // BRIGHT_RED_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_RED);
    case 95: // BRIGHT_MAGENTA_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_MAGENTA);
    case 96: // BRIGHT_BROWN_FG
        return cds_with_fg_color(cds, CDC_BRIGHT_BROWN);
    case 97: // BRIGHT_LIGHT_GREY_FG
        return cds_with_fg_color(cds, CDC_WHITE);
    case 40: // BLACK_BG
        return cds_with_bg_color(cds, CDC_BLACK);
    case 41: // BLUE_BG
        return cds_with_bg_color(cds, CDC_BLUE);
    case 42: // GREEN_BG
        return cds_with_bg_color(cds, CDC_GREEN);
    case 43: // CYAN_BG
        return cds_with_bg_color(cds, CDC_CYAN);
    case 44: // RED_BG
        return cds_with_bg_color(cds, CDC_RED);
    case 45: // MAGENTA_BG
        return cds_with_bg_color(cds, CDC_MAGENTA);
    case 46: // BROWN_BG
        return cds_with_bg_color(cds, CDC_BROWN);
    case 47: // LIGHT_GREY_BG
        return cds_with_bg_color(cds, CDC_LIGHT_GREY);
    case 100: // BRIGHT_BLACK_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_BLACK);
    case 101: // BRIGHT_BLUE_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_BLUE);
    case 102: // BRIGHT_GREEN_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_GREEN);
    case 103: // BRIGHT_CYAN_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_CYAN);
    case 104: // BRIGHT_RED_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_RED);
    case 105: // BRIGHT_MAGENTA_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_MAGENTA);
    case 106: // BRIGHT_BROWN_BG
        return cds_with_bg_color(cds, CDC_BRIGHT_BROWN);
    case 107: // BRIGHT_LIGHT_GREY_BG
        return cds_with_bg_color(cds, CDC_WHITE);
    }
}

void init_char_display_base(char_display_t *base, const char_display_impl_t *impl, size_t rows, size_t cols,
        char_display_style_t default_style) {
    *(const char_display_impl_t **)&(base->impl) = impl;

    *(size_t *)&(base->rows) = rows;
    *(size_t *)&(base->cols) = cols;

    *(char_display_style_t *)&(base->default_style) = default_style;

    base->cursor_row = 0;
    base->cursor_col = 0;
    base->curr_style = default_style;
}

static void cd_flip_cursor(char_display_t *cd) {
    char_display_style_t style;
    char c; 

    cd->impl->cd_get_c(cd, cd->cursor_row, cd->cursor_col, &style, &c);
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
    if (c) { // Only consume if it isn't the NULL terminator.
             // The NULL terminator will pass through the below 
             // switch statement.
        i++;
    }

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
        if (arg0 == 0) { // Clear from cursor to the end of the line.
            for (size_t col = cd->cursor_col; col < cd->cols; col++) {
                cd->impl->cd_set_c(cd, cd->cursor_row, col, cd->default_style, ' ');
            }
        } else if (arg0 == 1) { // Clear from beginning of the line to cursor.
            for (size_t col = 0; col <= cd->cursor_col; col++) {
                cd->impl->cd_set_c(cd, cd->cursor_row, col, cd->default_style, ' ');
            }
        } else if (arg0 == 2) { // Clear full line.
            cd->impl->cd_set_row(cd, cd->cursor_row, cd->default_style, ' ');
        }
        break;

    case 'G': { // Set Cursor col (1-indexed)
        size_t new_col = arg0 - 1;
        if (new_col >= cd->cols) {
            new_col = cd->cols - 1;
        }

        cd->cursor_col = new_col;
        break;
    }

    case 'H': { // Set cursor position
        size_t new_row = arg0 - 1;
        if (new_row >= cd->rows) {
            new_row = cd->rows - 1;
        }

        size_t new_col = arg1 - 1;
        if (new_col >= cd->cols) {
            new_col = cd->cols - 1;
        }

        cd->cursor_row = new_row;
        cd->cursor_col = new_col;

        break;
    }

    case 'S': // Scroll up
        cd->impl->cd_scroll_up(cd, arg0);
        break;

    case 'T': // Scroll Down.
        cd->impl->cd_scroll_down(cd, arg0);
        break;

    case 'm': // Styles!
        if (arg0 == 0) { // Reset case is slightly special.
            cd->curr_style = cd->default_style;
        } else {
            cd->curr_style = cds_with_ansi_style_code(cd->curr_style, arg0);
        }

        break;
    default: // Default does nothing!
        break;
    }

    return i - s; 
}

void cd_put_s(char_display_t *cd, const char *s) {
    const char *iter = s;
    char c;

    while ((c = *iter)) {
        iter++; // We'll consume `c` now to make my life easier.

        switch (c) {

        case '\n':
            cd_cursor_next_line(cd, 1);
            break;

        case '\r':
            cd->cursor_col = 0;
            break;

        case '\x1B':
            iter += cd_interp_esc_seq(cd, iter);
            break;

        default: {
            char c_to_print = 32 <= c && c < 127 ? c : ' ';
            cd->impl->cd_set_c(cd, cd->cursor_row, cd->cursor_col, cd->curr_style, c_to_print);
            cd_cursor_advance(cd);        
            break;
        }

        }
    }
}
