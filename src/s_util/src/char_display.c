
#include "s_util/char_display.h"

void init_char_display_base(char_display_t *base, const char_display_impl_t *impl, size_t rows, size_t cols, 
        char_display_style_t default_style) {
    *(const char_display_impl_t **)&(base->impl) = impl;
    *(size_t *)&(base->rows) = rows;
    *(size_t *)&(base->cols) = cols;

    base->cursor_row = 0;
    base->cursor_col = 0;

    base->default_style = default_style;
    base->curr_style = default_style;

    base->cursor_visible = true;
}

/**
 * This is a quick helper, it just flips the colors at the cursor location.
 * This does nothing if `cursor_visible` is false.
 */
static void cd_flip_cursor(char_display_t *cd) {
    if (cd->cursor_visible) {
        char_display_style_t style;
        char c = cd->impl->cd_get_c(cd, cd->cursor_row, cd->cursor_col, &style);

        cd->impl->cd_put_c(cd, cd->cursor_row, cd->cursor_col, cds_flip(style), c);
    }
}

void cd_put_c(char_display_t *cd, char c) {
    cd_flip_cursor(cd);

    switch (c) {
    case '\n':
        if (cd->cursor_row + 1 < cd->rows) {
            cd->cursor_row++; // Only increase cursor row if we can.
        } else {
            // Otherwise scroll the screen down one line.
            cd->impl->cd_scroll_down(cd, 1, ' ', cd->default_style);
        }

        cd->cursor_col = 0;
        break;

    case '\r': // Move the cursor to the beginning of the current line.
        cd->cursor_col = 0;
        break;

    default:
        if (32 <= c && c < 127) {
            cd->impl->cd_put_c(cd, cd->cursor_row, cd->cursor_col, cd->curr_style, c);
        }

        cd->cursor_col++;

        if (cd->cursor_col == cd->cols) { // Check to see if we've made it to the end of a line.
            if (cd->cursor_row + 1 < cd->rows) {
                cd->cursor_row++; // Only increase cursor row if we can.
            } else {
                // Otherwise scroll the screen down one line.
                cd->impl->cd_scroll_down(cd, 1, ' ', cd->default_style);
            }

            cd->cursor_col = 0;
        }
        break;
    }

    cd_flip_cursor(cd);
}

void cd_put_s(char_display_t *cd, const char *s) {

}
