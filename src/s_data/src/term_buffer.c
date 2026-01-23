
#include "s_data/term_buffer.h"
#include "s_util/str.h"

term_style_t ts_with_ansi_style_code(term_style_t ts, uint8_t ansi_style_code) {
    switch (ansi_style_code) {
    default:
    case 0: // Reset
        return term_style(TC_WHITE, TC_BLACK);
    case 1: // BOLD
        return ts | TS_BOLD;
    case 3: // ITALIC
        return ts | TS_ITALIC;
    case 30: // BLACK_FG
        return ts_with_fg_color(ts, TC_BLACK);
    case 31: // BLUE_FG
        return ts_with_fg_color(ts, TC_BLUE);
    case 32: // GREEN_FG
        return ts_with_fg_color(ts, TC_GREEN);
    case 33: // CYAN_FG
        return ts_with_fg_color(ts, TC_CYAN);
    case 34: // RED_FG
        return ts_with_fg_color(ts, TC_RED);
    case 35: // MAGENTA_FG
        return ts_with_fg_color(ts, TC_MAGENTA);
    case 36: // BROWN_FG
        return ts_with_fg_color(ts, TC_BROWN);
    case 37: // LIGHT_GREY_FG
        return ts_with_fg_color(ts, TC_LIGHT_GREY);
    case 90: // BRIGHT_BLACK_FG
        return ts_with_fg_color(ts, TC_BRIGHT_BLACK);
    case 91: // BRIGHT_BLUE_FG
        return ts_with_fg_color(ts, TC_BRIGHT_BLUE);
    case 92: // BRIGHT_GREEN_FG
        return ts_with_fg_color(ts, TC_BRIGHT_GREEN);
    case 93: // BRIGHT_CYAN_FG
        return ts_with_fg_color(ts, TC_BRIGHT_CYAN);
    case 94: // BRIGHT_RED_FG
        return ts_with_fg_color(ts, TC_BRIGHT_RED);
    case 95: // BRIGHT_MAGENTA_FG
        return ts_with_fg_color(ts, TC_BRIGHT_MAGENTA);
    case 96: // BRIGHT_BROWN_FG
        return ts_with_fg_color(ts, TC_BRIGHT_BROWN);
    case 97: // BRIGHT_LIGHT_GREY_FG
        return ts_with_fg_color(ts, TC_WHITE);
    case 40: // BLACK_BG
        return ts_with_bg_color(ts, TC_BLACK);
    case 41: // BLUE_BG
        return ts_with_bg_color(ts, TC_BLUE);
    case 42: // GREEN_BG
        return ts_with_bg_color(ts, TC_GREEN);
    case 43: // CYAN_BG
        return ts_with_bg_color(ts, TC_CYAN);
    case 44: // RED_BG
        return ts_with_bg_color(ts, TC_RED);
    case 45: // MAGENTA_BG
        return ts_with_bg_color(ts, TC_MAGENTA);
    case 46: // BROWN_BG
        return ts_with_bg_color(ts, TC_BROWN);
    case 47: // LIGHT_GREY_BG
        return ts_with_bg_color(ts, TC_LIGHT_GREY);
    case 100: // BRIGHT_BLACK_BG
        return ts_with_bg_color(ts, TC_BRIGHT_BLACK);
    case 101: // BRIGHT_BLUE_BG
        return ts_with_bg_color(ts, TC_BRIGHT_BLUE);
    case 102: // BRIGHT_GREEN_BG
        return ts_with_bg_color(ts, TC_BRIGHT_GREEN);
    case 103: // BRIGHT_CYAN_BG
        return ts_with_bg_color(ts, TC_BRIGHT_CYAN);
    case 104: // BRIGHT_RED_BG
        return ts_with_bg_color(ts, TC_BRIGHT_RED);
    case 105: // BRIGHT_MAGENTA_BG
        return ts_with_bg_color(ts, TC_BRIGHT_MAGENTA);
    case 106: // BRIGHT_BROWN_BG
        return ts_with_bg_color(ts, TC_BRIGHT_BROWN);
    case 107: // BRIGHT_LIGHT_GREY_BG
        return ts_with_bg_color(ts, TC_WHITE);
    }
}

term_buffer_t *new_term_buffer(allocator_t *al, term_cell_t default_cell, uint16_t rows, uint16_t cols) {
    term_buffer_t *tb = al_malloc(al, sizeof(term_buffer_t));

    term_cell_t *buf = NULL;
    if (rows > 0 && cols > 0) {
        buf = al_malloc(al, sizeof(term_cell_t) * rows * cols);
        if (!buf) {
            al_free(al, tb);
            return NULL;
        }
    }

    *(allocator_t **)&(tb->al) = al;
    *(term_cell_t *)&(tb->default_cell) = default_cell;
    tb->curr_style = tb->default_cell.style;
    tb->rows = rows;
    tb->cols = cols;
    tb->cursor_row = 0;
    tb->cursor_col = 0;
    tb->buf = buf;

    tb_default_clear(tb);

    return tb; 
}

void delete_term_buffer(term_buffer_t *tb) {
    if (tb && tb->al) {
        al_free(tb->al, tb->buf); 
        al_free(tb->al, tb);
    }
}

void tb_clear(term_buffer_t *tb, term_cell_t cell_val) {
    term_cell_t * const first_row = tb->buf;
    for (uint16_t c = 0; c < tb->cols; c++) {
        first_row[c] = cell_val;
    }

    for (uint16_t r = 1; r < tb->rows; r++) {
        mem_cpy(tb->buf + (tb->cols * r), first_row, tb->cols * sizeof(term_cell_t));
    }

    tb->cursor_row = 0;
    tb->cursor_col = 0;
}

fernos_error_t tb_resize(term_buffer_t *tb, uint16_t rows, uint16_t cols) {
    if (!(tb->al)) {
        return FOS_E_STATE_MISMATCH;
    }

    size_t new_size = rows * cols * sizeof(term_cell_t);
    term_cell_t *new_buf = al_realloc(tb->al, tb->buf, new_size);

    if (new_size > 0 && !new_buf) {
        return FOS_E_NO_MEM;
    }

    // Success!

    tb->rows = rows;
    tb->cols = cols;
    tb->buf = new_buf;

    tb_clear(tb, tb->default_cell);

    return FOS_E_SUCCESS;
}

void tb_scroll_up(term_buffer_t *tb, uint16_t shift) {
    if (shift == 0) {
        return;
    }

    if (shift >= tb->rows) {
        tb_default_clear(tb);
        return;
    }

    const uint16_t left_over_rows = tb->rows - shift;

    for (uint16_t r = 0; r < left_over_rows; r++) {
        mem_cpy(tb->buf + (r * tb->cols), tb->buf + ((r + shift) * tb->cols), tb->cols * sizeof(term_cell_t));
    }

    // Given the shift is greater than 0, we know the final row will always need to be reset.
    term_cell_t * const final_row = tb->buf + ((tb->rows - 1) * tb->cols);

    for (uint16_t c = 0; c < tb->cols; c++) {
        final_row[c] = tb->default_cell;
    }

    // Now we can memcpy from the final row into other new rows needing to be reset.
    for (uint16_t r = left_over_rows; r < tb->rows - 1; r++) {
        mem_cpy(tb->buf + (r * tb->cols), final_row, tb->cols * sizeof(term_cell_t));
    }
}

void tb_scroll_down(term_buffer_t *tb, uint16_t shift) {
    // Same thing as scroll up, just needs to be reversed.

    if (shift == 0) {
        return;
    }

    if (shift >= tb->rows) {
        tb_default_clear(tb);
        return;
    }

    for (uint16_t r = tb->rows - 1; r >= shift; r--) {
        mem_cpy(tb->buf + (r * tb->cols), tb->buf + ((r - shift) * tb->cols), tb->cols * sizeof(term_cell_t));
    }

    // Given shift is greater than 0, first row must be reset.
    term_cell_t * const first_row = tb->buf;

    for (uint16_t c = 0; c < tb->cols; c++) {
        first_row[c] = tb->default_cell;
    }

    for (uint16_t r = 1; r < shift; r++) {
        mem_cpy(tb->buf + (r * tb->cols), first_row, tb->cols * sizeof(term_cell_t));
    }
}

/**
 * Bring the cursor up `n` lines, scrolling if needed.
 * (Cursor moves to the beginning of the new line)
 */
static void tb_cursor_prev_line(term_buffer_t *tb, size_t n) {
    if (tb->cursor_row < n) {
        tb_scroll_down(tb, n - tb->cursor_row);
        tb->cursor_row = 0; 
    } else {
        tb->cursor_row -= n;
    }

    tb->cursor_col = 0;
}

/**
 * Bring the cursor down `n` lines, scrolling if needed.
 * (Cursor moves to the beginning of the new line)
 */
static void tb_cursor_next_line(term_buffer_t *tb, uint16_t n) {
    size_t dest_row = tb->cursor_row + n;
    if (dest_row >= tb->rows) {
        tb_scroll_up(tb, dest_row - (tb->rows - 1));
        tb->cursor_row = tb->rows - 1;
    } else {
        tb->cursor_row = dest_row;
    }

    tb->cursor_col = 0;
}

static void tb_cursor_advance(term_buffer_t *tb) {
    if (tb->cursor_col == tb->cols - 1) {
        tb_cursor_next_line(tb, 1);
    } else {
        tb->cursor_col++;
    }
}

void tb_put_c(term_buffer_t *tb, char c) {
    switch (c) {
    case '\n':
        tb_cursor_next_line(tb, 1);
        break;
    case '\r':
        tb->cursor_col = 0;
        break;
    case '\b':
        if (tb->cursor_col > 0) {
            tb->cursor_col--;
        } else if (tb->cursor_row > 0) {
            tb->cursor_row--;
            tb->cursor_col = tb->cols - 1;
        }
        break;
    default: {
        char c_to_print = 32 <= c && c < 127 ? c : ' ';
        tb->buf[(tb->cursor_row * tb->cols) + tb->cursor_col] = (term_cell_t) {
            .c = c_to_print,
            .style = tb->curr_style
        };
        tb_cursor_advance(tb);        
        break;
    }

    }
}

/**
 * Iterpret an ANSI escape sequence. This assume that `*s` points to the
 * first character after `\x1B`.
 *
 * Returns the number of characters consumed.
 */
static size_t tb_interp_esc_seq(term_buffer_t *tb, const char *s) {
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
        if (tb->cursor_row < arg0) {
            tb->cursor_row = 0;
        } else {
            tb->cursor_row -= arg0;
        }
        break;
    case 'B': // Cursor Down.
        if (tb->cursor_row + arg0 >= tb->rows) {
            tb->cursor_row = tb->rows - 1;
        } else {
            tb->cursor_row += arg0;
        }
        break;
    case 'C': // Cursor Right.
        if (tb->cursor_col + arg0 >= tb->cols) {
            tb->cursor_col = tb->cols - 1;
        } else {
            tb->cursor_col += arg0;
        }
        break;
    case 'D': // Cursor Left.
        if (tb->cursor_col < arg0) {
            tb->cursor_col = 0;
        } else {
            tb->cursor_col -= arg0;
        }
        break;

    /*
     * Next/Previous Line.
     */

    case 'E': // Cursor Next Line.
        tb_cursor_next_line(tb, arg0);
        break;

    case 'F': // Cursor Previous Line.
        tb_cursor_prev_line(tb, arg0);
        break;

    case 'J': // Cursor Clear Display.
        if (arg0 == 2) {
            tb_default_clear(tb);
        }
        break;

    case 'K': { // Clear line options
        term_cell_t * const row = tb->buf + (tb->cursor_row * tb->cols);

        if (arg0 == 0) { // Clear from cursor to the end of the line.
            for (size_t col = tb->cursor_col; col < tb->cols; col++) {
                row[col] = tb->default_cell;
            }
        } else if (arg0 == 1) { // Clear from beginning of the line to cursor.
            for (size_t col = 0; col <= tb->cursor_col; col++) {
                row[col] = tb->default_cell;
            }
        } else if (arg0 == 2) { // Clear full line.
            for (size_t col = 0; col <= tb->cols; col++) {
                row[col] = tb->default_cell;
            }
        }

        break;
    }

    case 'G': { // Set Cursor col (1-indexed)
        size_t new_col = arg0 - 1;
        if (new_col >= tb->cols) {
            new_col = tb->cols - 1;
        }

        tb->cursor_col = new_col;
        break;
    }

    case 'H': { // Set cursor position
        size_t new_row = arg0 - 1;
        if (new_row >= tb->rows) {
            new_row = tb->rows - 1;
        }

        size_t new_col = arg1 - 1;
        if (new_col >= tb->cols) {
            new_col = tb->cols - 1;
        }

        tb->cursor_row = new_row;
        tb->cursor_col = new_col;

        break;
    }

    case 'S': // Scroll up
        tb_scroll_up(tb, arg0);
        break;

    case 'T': // Scroll Down.
        tb_scroll_down(tb, arg0);
        break;

    case 'm': // Styles!
        if (arg0 == 0) { // Reset case is slightly special.
            tb->curr_style = tb->default_cell.style;
        } else {
            tb->curr_style = ts_with_ansi_style_code(tb->curr_style, arg0);
        }

        break;
    default: // Default does nothing!
        break;
    }

    return i - s; 
}

void tb_put_s(term_buffer_t *tb, const char *s) {
    const char *iter = s;
    char c;

    while ((c = *iter)) {
        iter++; // We'll consume `c` now to make my life easier.

        switch (c) {

        case '\n':
            tb_cursor_next_line(tb, 1);
            break;

        case '\r':
            tb->cursor_col = 0;
            break;

        case '\b':
            if (tb->cursor_col > 0) {
                tb->cursor_col--;
            } else if (tb->cursor_row > 0) {
                tb->cursor_row--;
                tb->cursor_col = tb->cols - 1;
            }
            break;

        case '\x1B':
            iter += tb_interp_esc_seq(tb, iter);
            break;

        default: {
            char c_to_print = 32 <= c && c < 127 ? c : ' ';
            tb->buf[(tb->cursor_row * tb->cols) + tb->cursor_col] = (term_cell_t) {
                .style = tb->curr_style, .c = c_to_print
            };
            tb_cursor_advance(tb);        
            break;
        }

        }
    }
}

void tb_put_fmt_s(term_buffer_t *tb, const char *fmt, ...) {
    char tb_fmt_buf[CHAR_DISPLAY_FMT_BUF_SIZE];

    va_list va;
    va_start(va, fmt);
    str_vfmt(tb_fmt_buf, fmt, va);
    va_end(va);

    tb_put_s(tb, tb_fmt_buf);
}

