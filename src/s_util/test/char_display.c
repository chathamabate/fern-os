
#include "s_util/char_display.h"
#include "s_util/ansi.h"
#include "s_util/str.h"
#include "s_util/rand.h"
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
    // The below few lines construct a fresh dummy display.
    // We know that after calling the base constructor, the cursor will have value (0, 0).
    // After we must clear the grid and flip the style at the cursor position.

    init_char_display_base(dcd, &DUMMY_CHAR_DISPLAY_IMPL, GRID_ROWS, GRID_COLS, 
            char_display_style(CDC_WHITE, CDC_BLACK)); 

    for (size_t row = 0; row < dcd->rows; row++) {
        for (size_t col = 0; col < dcd->cols; col++) {
            grid[row][col] = (char_display_pair_t) {
                .style = dcd->default_style,
                .c = ' '
            };
        }
    }

    grid[0][0].style = cds_flip(grid[0][0].style);

    TEST_SUCCEED();
}

static bool test_put_c0(void) {
    // Here let's just test with no special codes.

    const size_t shift = 4;
    const size_t rows_to_print = (dcd->rows - 1) + shift;

    for (size_t row = 0; row < rows_to_print; row++) {
        for (size_t col = 0; col < dcd->cols; col++) {
            cd_put_c(dcd, 'a' + ((row + col) % 26));
        }
    }

    // We printed out more rows than the display has, let's make sure all 
    // characters make sense.

    for (size_t row = 0; row < dcd->rows - 1; row++) {
        const size_t true_row = row + shift; // The first `shift` rows should've been scrolled
                                             // off the screen.
        for (size_t col = 0; col < dcd->cols; col++) {
            TEST_EQUAL_HEX('a' + ((true_row + col) % 26), (uint8_t)(grid[row][col].c));
            TEST_EQUAL_HEX(dcd->default_style, grid[row][col].style);
        }
    }

    for (size_t col = 0; col < dcd->cols; col++) {
        TEST_EQUAL_HEX(' ', (uint8_t)(grid[dcd->rows - 1][col].c));

        const char_display_style_t exp_style = col == 0 
            ? cds_flip(dcd->default_style) 
            : dcd->default_style;

        TEST_EQUAL_HEX(exp_style, grid[dcd->rows - 1][col].style);
    }

    TEST_SUCCEED();
}

static bool test_put_c1(void) {
    // Now try the two control sequences '\r' and '\n'  

    const size_t cols_to_write = 10;
    TEST_TRUE(cols_to_write < dcd->cols); // Must have at least one space 
                                          // at the end of the line.

    for (size_t iter = 0; iter < cols_to_write; iter++) {
        for (size_t col = 0; col < cols_to_write - iter; col++) {
            cd_put_c(dcd, 'a' + iter);
        }
        cd_put_c(dcd, '\r');
    }

    for (size_t col = 0; col < cols_to_write; col++) {
        TEST_EQUAL_HEX('a' + (cols_to_write - 1 - col), (uint8_t)grid[0][col].c);
    }

    cd_put_c(dcd, '\n');
    cd_put_c(dcd, 'B');
    TEST_EQUAL_HEX('B', grid[1][0].c);

    // Try to force a single scroll up.
    for (size_t i = 0; i < dcd->rows - 1; i++) {
        cd_put_c(dcd, '\n');
    }

    TEST_EQUAL_HEX('B', grid[0][0].c);

    TEST_SUCCEED();
}

static bool test_simple_cursor_movements(void) {
    // We'll confirm the cursor moved by seeing if the color of the destination cell
    // flipped. This is gonna be a random test btw!!!

    size_t row = 0;
    size_t col = 0;

    rand_t r = rand(0);
    char buf[32];

    for (size_t i = 0; i < 100; i++) {
        uint8_t amt = next_rand_u8(&r) % 16;

        size_t new_row = row;
        size_t new_col = col;

        if (next_rand_u1(&r)) {
            if (next_rand_u1(&r)) { // Up
                cd_put_s(dcd, ansi_cursor_up_n(buf, amt));
                new_row = amt > row ? 0 : row - amt;
            } else { // Down
                cd_put_s(dcd, ansi_cursor_down_n(buf, amt));
                new_row = amt + row >= dcd->rows ? dcd->rows - 1 : amt + row;
            }
        } else {
            if (next_rand_u1(&r)) { // Left
                cd_put_s(dcd, ansi_cursor_left_n(buf, amt));
                new_col = amt > col ? 0 : col - amt;
            } else { // Right
                cd_put_s(dcd, ansi_cursor_right_n(buf, amt));
                new_col = amt + col >= dcd->cols ? dcd->cols - 1 : amt + col;
            }
        }

        if (new_row != row || new_col != col) {
            TEST_EQUAL_HEX(dcd->default_style, grid[row][col].style);
        }

        TEST_EQUAL_HEX(cds_flip(dcd->default_style), 
                grid[new_row][new_col].style);

        row = new_row;
        col = new_col;
    }

    TEST_SUCCEED();
}

static bool test_next_and_prev_line(void) {
    TEST_TRUE(dcd->rows >= 4);

    // Put a character in every row but the last.
    for (size_t row = 0; row < dcd->rows - 1; row++) {
        cd_put_c(dcd, 'a' + (row % 26));
        cd_put_s(dcd, "\n"); // Quick put s cntl character test.
    }

    char buf[16];

    // We want to test scroll.
    cd_put_s(dcd, ansi_previous_line_n(buf, dcd->rows));

    // Now there is one new line on the top.

    TEST_EQUAL_HEX(' ', grid[0][0].c);
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[0][0].style);

    for (size_t row = 1; row < dcd->rows; row++) {
        TEST_EQUAL_HEX('a' + ((row - 1) % 26), (uint8_t)(grid[row][0].c));
    }

    cd_put_s(dcd, ansi_previous_line_n(buf, 2));
    
    // Now there are 3 new lines at the top. (2 cut off from the bottom)

    cd_put_s(dcd, ansi_next_line_n(buf, 3));
    TEST_EQUAL_HEX('a', (uint8_t)(grid[3][0].c));
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[3][0].style);

    cd_put_s(dcd, ansi_next_line_n(buf, dcd->rows - 3));

    // Cursor is at a bottom empty line.

    TEST_EQUAL_HEX(' ', (uint8_t)(grid[dcd->rows - 1][0].c));
    TEST_EQUAL_HEX('a' + ((dcd->rows - 4) % 26), (uint8_t)(grid[dcd->rows - 2][0].c));

    TEST_SUCCEED();
}

static bool test_clear_options(void) {
    cd_put_s(dcd, "Hello\r" ANSI_CLEAR_TO_BOL);
    TEST_EQUAL_HEX(' ', grid[0][0].c);
    TEST_EQUAL_HEX('e', grid[0][1].c);

    cd_put_s(dcd, ANSI_CURSOR_RIGHT ANSI_CURSOR_RIGHT ANSI_CLEAR_TO_EOL);
    TEST_EQUAL_HEX('e', grid[0][1].c);
    TEST_EQUAL_HEX(' ', grid[0][2].c);
    TEST_EQUAL_HEX(' ', grid[0][3].c);

    cd_put_s(dcd, "aye" ANSI_CLEAR_LINE);
    for (size_t i = 0; i < dcd->cols; i++) {
        TEST_EQUAL_HEX(' ', grid[0][i].c);
    }

    TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[0][5].style);

    cd_put_s(dcd, "WOO\nAYE YO\nTOOOBO\n" ANSI_CLEAR_DISPLAY);

    for (size_t row = 0; row < dcd->rows; row++) {
        for (size_t col = 0; col < dcd->cols; col++) {
            TEST_EQUAL_HEX(' ', grid[row][col].c);
            const char_display_style_t exp_style = row == 0 && col == 0 
                ? cds_flip(dcd->default_style) 
                : dcd->default_style;
            TEST_EQUAL_HEX(exp_style, grid[row][col].style);
        }
    }

    TEST_SUCCEED();
}

static bool test_set_cursor_position(void) {
    char buf[16];

    for (size_t col = 1; col <= dcd->cols; col++) { // 1-indexed loop
        cd_put_s(dcd, ansi_set_cursor_col(buf, col)); 
        TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[0][col - 1].style);
    }

    cd_put_s(dcd, ansi_set_cursor_col(buf, dcd->cols * 2));
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[0][dcd->cols - 1].style);

    for (size_t col = 0; col < dcd->cols - 1; col++) {
        TEST_EQUAL_HEX(dcd->default_style, grid[0][col].style);
    }

    // Now test set cursor position... RANDOM!

    cd_put_c(dcd, '\r');

    rand_t r = rand(0);

    size_t cursor_row = 0; 
    size_t cursor_col = 0;

    for (size_t iter = 0; iter < 100; iter++) {
        size_t new_row = next_rand_u8(&r) % (dcd->rows + 10);
        size_t new_col = next_rand_u8(&r) % (dcd->cols + 10);

        cd_put_s(dcd, ansi_set_cursor_position(buf, new_row + 1, new_col + 1));

        if (new_row >= dcd->rows) {
            new_row = dcd->rows - 1;
        }

        if (new_col >= dcd->cols) {
            new_col = dcd->cols - 1;
        }

        if (new_row != cursor_row || new_col != cursor_col) {
            TEST_EQUAL_HEX(dcd->default_style, grid[cursor_row][cursor_col].style);
        }

        TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[new_row][new_col].style);

        cursor_row = new_row;
        cursor_col = new_col;
    }

    TEST_SUCCEED();
}

static bool test_scroll_up_and_down(void) {
    // The behavior of scrolling as been tested in many ways already.
    // This is really just testing that the scroll escape sequence is interpreted correctly.

    TEST_TRUE(dcd->rows >= 6);

    cd_put_s(dcd, "A\r");

    char buf[16];

    cd_put_s(dcd, ansi_scroll_down_n(buf, 5));
    TEST_EQUAL_HEX('A', grid[5][0].c);

    cd_put_s(dcd, ansi_scroll_up_n(buf, 2));
    TEST_EQUAL_HEX('A', grid[3][0].c);
    TEST_EQUAL_HEX(dcd->default_style, grid[3][0].style);

    TEST_EQUAL_HEX(cds_flip(dcd->default_style), grid[0][0].style);

    TEST_SUCCEED();
}

static bool test_styles(void) {
    cd_put_s(dcd, ANSI_BLUE_FG ANSI_BLACK_BG "A" ANSI_ITALIC "B" ANSI_RED_BG "C" 
            ANSI_GREEN_FG "D" ANSI_RESET "E");

    const char_display_pair_t exp_pairs[] = {
        {char_display_style(CDC_BLUE, CDC_BLACK), 'A'},
        {CDS_ITALIC | char_display_style(CDC_BLUE, CDC_BLACK), 'B'},
        {CDS_ITALIC | char_display_style(CDC_BLUE, CDC_RED), 'C'},
        {CDS_ITALIC | char_display_style(CDC_GREEN, CDC_RED), 'D'},
        {dcd->default_style, 'E'},
    };
    const size_t num_exp_pairs = sizeof(exp_pairs) / sizeof(exp_pairs[0]);

    for (size_t i = 0; i < num_exp_pairs; i++) {
        TEST_EQUAL_HEX(exp_pairs[i].style, grid[0][i].style);
        TEST_EQUAL_HEX(exp_pairs[i].c, grid[0][i].c);
    }

    TEST_SUCCEED();
}

bool test_char_display(void) {
    BEGIN_SUITE("Character Display");
    RUN_TEST(test_put_c0);
    RUN_TEST(test_put_c1);
    RUN_TEST(test_simple_cursor_movements);
    RUN_TEST(test_next_and_prev_line);
    RUN_TEST(test_clear_options);
    RUN_TEST(test_set_cursor_position);
    RUN_TEST(test_scroll_up_and_down);
    RUN_TEST(test_styles);
    return END_SUITE();
}

