
#include "s_util/ansi.h"
#include "s_util/rand.h"
#include "k_startup/gfx.h"

static bool posttest(void);
static bool pretest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

/*
 * WARNING THIS IS HIGHLY CYCLIC!
 *
 * `gfx_direct_put_fmt_s` uses a terminal buffer for printing!
 *
 * If the tests pass though, we'd expect everything to be printed out correctly.
 */

#define LOGF_METHOD(...) gfx_direct_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_UINT(num_al_blocks, al_num_user_blocks(get_default_allocator()));
    TEST_SUCCEED();
}

/*
 * These tests assume direct access of the buf, cols, and rows fields is OK.
 *
 * (Which is intended!)
 */

static bool test_tb_new_and_delete(void) {
    const term_cell_t dc = {
        .style = term_style(TC_WHITE, TC_BLACK),
        .c = ' '
    };

    const uint16_t rows = 10;
    const uint16_t cols = 20;

    term_buffer_t *tb = new_da_term_buffer(
        dc, rows, cols
    );

    TEST_EQUAL_UINT(rows, tb->rows);
    TEST_EQUAL_UINT(cols, tb->cols);

    TEST_TRUE(tb != NULL);

    // All cells should start as default!
    const uint32_t buf_size = tb->rows * tb->cols;
    for (uint32_t ci = 0; ci < buf_size; ci++) {
        TEST_TRUE(tc_equals(dc, tb->buf[ci]));
    }

    delete_term_buffer(tb);

    TEST_SUCCEED();
}

static bool test_tb_clear(void) {
    term_buffer_t *tb = new_da_term_buffer(
        (term_cell_t) {
            .style = term_style(TC_WHITE, TC_BLACK),
            .c = ' ' 
        },
        10, 20
    );

    TEST_TRUE(tb != NULL);

    const term_cell_t nc = {
        .style = term_style(TC_RED, TC_BLACK),
        .c = 'x'
    };

    tb_clear(tb, nc);
    const uint32_t buf_size = tb->rows * tb->cols;
    for (uint32_t ci = 0; ci < buf_size; ci++) {
        TEST_TRUE(tc_equals(nc, tb->buf[ci]));
    }

    delete_term_buffer(tb);

    TEST_SUCCEED();
}

static bool test_tb_resize(void) {
    const term_cell_t dc = {
        .style = term_style(TC_WHITE, TC_BLACK),
        .c = ' '
    };

    term_buffer_t *tb = new_da_term_buffer(
        dc,
        10, 20
    );

    TEST_TRUE(tb != NULL);

    tb_clear(tb, (term_cell_t) {
        .style = term_style(TC_WHITE, TC_BLACK),
        .c = 'x' 
    });

    const uint16_t new_rows = 20;
    const uint16_t new_cols = 30;

    TEST_SUCCESS(tb_resize(tb, new_rows, new_cols));

    TEST_EQUAL_UINT(new_rows, tb->rows);
    TEST_EQUAL_UINT(new_cols, tb->cols);

    const uint32_t buf_size = tb->rows * tb->cols;
    for (uint32_t ci = 0; ci < buf_size; ci++) {
        TEST_TRUE(tc_equals(tb->default_cell, tb->buf[ci]));
    }

    delete_term_buffer(tb);

    TEST_SUCCEED();
}

static bool test_scroll(void) {
    // Pretty overkill test here, but whatever.
    term_buffer_t *tb = new_da_term_buffer(
        (term_cell_t) {
            .style = term_style(TC_WHITE, TC_BLACK),
            .c = ' ' 
        },
        10, 20
    );

    // We are going be using letters for this test.
    TEST_TRUE(tb->rows <= 26);

    TEST_TRUE(tb != NULL);

    const int16_t cases[] = { 
        // + For scroll down.
        // - For scroll up.

        0
    };
    const size_t num_cases = sizeof(cases) / sizeof(cases[0]);

    for (size_t ci = 0; ci < num_cases; ci++) {
        int16_t scroll = cases[ci];
        TEST_TRUE(scroll != INT16_MIN); // invalid case!
        
        // First let's reset our buffer.
        
        for (uint16_t r = 0; r < tb->rows; r++) {
            term_cell_t * const row = tb->buf + (r * tb->cols);
            for (uint16_t c = 0; c < tb->cols; c++) {
                row[c] = (term_cell_t) {
                    .c = 'a' + r,
                    .style = term_style(TC_WHITE, TC_BLACK)
                };
            }
        }

        if (scroll < 0) {
            scroll *= -1;
            tb_scroll_up(tb, scroll);
            const uint16_t exp_scroll = MIN(tb->rows, scroll);

            for (uint16_t r = 0; r < tb->rows; r++) {
                term_cell_t * const row = tb->buf + (r * tb->cols);
                for (uint16_t c = 0; c < tb->cols; c++) {
                    if (r < tb->rows - exp_scroll) {
                        TEST_EQUAL_UINT('a' + r + exp_scroll, row[c].c);
                    } else {
                        TEST_EQUAL_UINT(' ', row[c].c);
                    }
                }
            }
        } else {
            tb_scroll_down(tb, scroll);
            const uint16_t exp_scroll = MIN(tb->rows, scroll);

            for (uint16_t r = 0; r < tb->rows; r++) {
                term_cell_t * const row = tb->buf + (r * tb->cols);
                for (uint16_t c = 0; c < tb->cols; c++) {
                    if (r >= exp_scroll) {
                        TEST_EQUAL_UINT('a' + r - exp_scroll, row[c].c);
                    } else {
                        TEST_EQUAL_UINT(' ', row[c].c);
                    }
                }
            }
        }
    }

    delete_term_buffer(tb);

    TEST_SUCCEED();
}


bool test_term_buffer(void) {
    BEGIN_SUITE("Terminal Buffer");
    RUN_TEST(test_tb_new_and_delete);
    RUN_TEST(test_tb_clear);
    RUN_TEST(test_tb_resize);
    RUN_TEST(test_scroll);
    return END_SUITE();
}


/*
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
    // Now try the two control sequences '\r', '\n', and '\b'

    const size_t cols_to_write = 10;
    TEST_TRUE(cols_to_write < dcd->cols); // Must have at least one space 
                                          // at the end of the line.

    // An initial backspace should do nothing.
    cd_put_c(dcd, '\b');
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), 
            grid[0][0].style);

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

    cd_put_c(dcd, '\b');
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), 
            grid[dcd->rows - 2][dcd->cols - 1].style);

    cd_put_c(dcd, '\b');
    TEST_EQUAL_HEX(cds_flip(dcd->default_style), 
            grid[dcd->rows - 2][dcd->cols - 2].style);

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

static bool test_bad_sequences(void) {
    cd_put_s(dcd, ANSI_CSI);
    TEST_EQUAL_HEX(' ', grid[0][0].c);

    cd_put_s(dcd, "\r" ANSI_CSI "$@");
    TEST_EQUAL_HEX('@', grid[0][0].c);

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
    RUN_TEST(test_bad_sequences);
    return END_SUITE();
}
*/

