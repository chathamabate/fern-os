
#include "s_util/char_display.h"
#include "s_util/str.h"
#include "k_bios_term/term.h"

static bool pretest(void);

#define PRETEST() pretest()

// NOTE: This is a cyclic dependency, but we'll allow it.
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

/*
 * Testing a character display is kinda difficult, the best way is ultimately just to look
 * at the display (If it's visible that is)
 *
 * This test here DOES NOT generically test the virtual implementations of any character
 * display. IT INSTEAD just tests the helper logic found inside the char display base helper
 * functions. (Specifically, do ANSI codes and control characters work as expected)
 *
 * This test sets up a "dummy" character display which just writes all characters and styles
 * to a static 2D array `grid`. The following tests will inspect grid to confirm tests
 * are running correctly.
 */

#define GRID_ROWS 40
#define GRID_COLS 80

static char_display_pair_t grid[GRID_ROWS][GRID_COLS];

static char_display_t cd;

static void delete_dummy_char_display(char_display_t *dcd) { 
    // Nothing to delete.
    (void)dcd;
}

static void dcd_get_c(char_display_t *cd, size_t row, size_t col, char_display_style_t *style, char *c) {
    if (row < cd->rows && col < cd->cols) {
        if (style) {
            *style = grid[row][col].style;
        }
        if (c) {
            *c = grid[row][col].c;
        }
    }
}

static void dcd_set_c(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c) {
    if (row < cd->rows && col < cd->cols) {
        grid[row][col] = (char_display_pair_t) {
            .style = style,
            .c = c
        };
    }
}

static void dcd_scroll_up(char_display_t *cd, size_t shift) {
    if (shift == 0) {
        return;
    }

    if (shift > cd->rows) {
        shift = cd->rows;
    }

    for (size_t row = 0; row < cd->rows - shift; row++) {
        mem_cpy(grid[row], grid[row + shift], cd->cols * sizeof(char_display_pair_t));
    }

    for (size_t row = cd->rows - shift; row < cd->rows; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            grid[row][col] = (char_display_pair_t) {
                .style = cd->default_style,
                .c = ' '
            };
        }
    }
}

static void dcd_scroll_down(char_display_t *cd, size_t shift) {
    if (shift == 0) {
        return;
    }

    if (shift > cd->rows) {
        shift = cd->rows;
    }

    for (size_t row = cd->rows - 1; row != shift - 1; row--) {
        mem_cpy(grid[row], grid[row - shift], cd->cols * sizeof(char_display_pair_t));
    }

    for (size_t row = 0; row < shift; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            grid[row][col] = (char_display_pair_t) {
                .style = cd->default_style,
                .c = ' '
            };
        }
    }
}

static void dcd_set_row(char_display_t *cd, size_t row, char_display_style_t style, char c) {
    if (row < cd->rows) {
        for (size_t col = 0; col < cd->cols; col++) {
            grid[row][col] = (char_display_pair_t) {
                .style = style,
                .c = c
            };
        }
    }
}

static void dcd_set_grid(char_display_t *cd, char_display_style_t style, char c) {
    for (size_t row = 0; row < cd->rows; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            grid[row][col] = (char_display_pair_t) {
                .style = style,
                .c = c
            };
        }
    }
}

static const char_display_impl_t DUMMY_CHAR_DISPLAY_IMPL = {
    .delete_char_display = delete_dummy_char_display,
    .cd_set_c = dcd_set_c,
    .cd_get_c = dcd_get_c,
    .cd_scroll_up = dcd_scroll_up,
    .cd_scroll_down = dcd_scroll_down,
    .cd_set_row = dcd_set_row,
    .cd_set_grid = dcd_set_grid
};

static char_display_t _dcd;
static char_display_t * const dcd = &_dcd;

static bool pretest(void) {
    init_char_display_base(dcd, &DUMMY_CHAR_DISPLAY_IMPL, GRID_ROWS, GRID_COLS, 
            char_display_style(CDC_WHITE, CDC_BLACK)); 
}

bool test_char_display(void) {
    BEGIN_SUITE("Character Display");
    return END_SUITE();
}

