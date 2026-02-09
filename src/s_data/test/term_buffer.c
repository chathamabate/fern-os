
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
 *
 * Also, these used to be tests for the now deleted structure "char_display_t".
 * I basically quickly refactored them to the `term_buffer_t` interface.
 * This probs could've been done better, but at the same time, these tests are arguably
 * very overkill.
 */

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"

static size_t num_al_blocks;
static term_buffer_t *tb;
static const term_cell_t dc = {
    .style = term_style(TC_WHITE, TC_BLACK),
    .c = ' '
};

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());
    tb = new_da_term_buffer(dc, 10, 20);
    TEST_TRUE(tb != NULL);
    TEST_SUCCEED();
}

static bool posttest(void) {
    delete_term_buffer(tb);
    TEST_EQUAL_UINT(num_al_blocks, al_num_user_blocks(get_default_allocator()));
    TEST_SUCCEED();
}

/*
 * These tests assume direct access of the buf, cols, rows, and cursor is OK.
 *
 * (Which is intended!)
 */

static bool test_tb_clear(void) {
    const term_cell_t nc = {
        .style = term_style(TC_RED, TC_BLACK),
        .c = 'x'
    };

    tb_clear(tb, nc);
    const uint32_t buf_size = tb->rows * tb->cols;
    for (uint32_t ci = 0; ci < buf_size; ci++) {
        TEST_TRUE(tc_equals(nc, tb->buf[ci]));
    }

    TEST_SUCCEED();
}

static bool test_tb_resize(void) {
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

    TEST_SUCCEED();
}

static bool test_scroll(void) {
    // Pretty overkill test here, but whatever.

    // We are going be using letters for this test.
    TEST_TRUE(tb->rows <= 26);

    TEST_TRUE(tb != NULL);

    const int16_t cases[] = { 
        // + For scroll down.
        // - For scroll up.

        0,
        1, -1,
        2, -2,
        5, -5,
        10, -10,
        50, -50
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

    TEST_SUCCEED();
}

static bool test_term_put_c0(void) {
    // Maybe just print too many characters and see what happens?
    // Here let's just test with no special codes.

    const size_t shift = 4;
    const size_t rows_to_print = (tb->rows - 1) + shift;

    for (size_t row = 0; row < rows_to_print; row++) {
        for (size_t col = 0; col < tb->cols; col++) {
            tb_put_c(tb, 'a' + ((row + col) % 26));
        }
    }

    // We printed out more rows than the display has, let's make sure all 
    // characters make sense.

    for (size_t row = 0; row < (size_t)(tb->rows - 1); row++) {
        const size_t true_row = row + shift; // The first `shift` rows should've been scrolled
                                             // off the screen.
        for (size_t col = 0; col < tb->cols; col++) {
            TEST_EQUAL_HEX('a' + ((true_row + col) % 26), (uint8_t)TB_CELL(tb, row, col).c);
            TEST_EQUAL_HEX(tb->default_cell.style, TB_CELL(tb, row, col).style);
        }
    }

    for (size_t col = 0; col < tb->cols; col++) {
        TEST_EQUAL_HEX(' ', (uint8_t)TB_CELL(tb, tb->rows - 1, col).c);
        TEST_EQUAL_HEX(tb->default_cell.style, TB_CELL(tb, tb->rows - 1, col).style);
    }

    TEST_SUCCEED();
}

static bool test_term_put_c1(void) {
    // Now try the two control sequences '\r', '\n', and '\b'
    const size_t cols_to_write = 10;
    TEST_TRUE(cols_to_write < tb->cols); // Must have at least one space 
                                          // at the end of the line.

    // An initial backspace should do nothing.
    tb_put_c(tb, '\b');

    for (size_t iter = 0; iter < cols_to_write; iter++) {
        for (size_t col = 0; col < cols_to_write - iter; col++) {
            tb_put_c(tb, 'a' + iter);
        }
        tb_put_c(tb, '\r');
    }

    for (size_t col = 0; col < cols_to_write; col++) {
        TEST_EQUAL_HEX('a' + (cols_to_write - 1 - col), (uint8_t)TB_CELL(tb, 0, col).c);
    }

    tb_put_c(tb, '\n');
    tb_put_c(tb, 'B');
    TEST_EQUAL_HEX('B', TB_CELL(tb, 1, 0).c);

    // Try to force a single scroll up.
    for (size_t i = 0; i < (size_t)(tb->rows - 1); i++) {
        tb_put_c(tb, '\n');
    }

    TEST_EQUAL_HEX('B', TB_CELL(tb, 0, 0).c);

    tb_put_c(tb, 'a');
    tb_put_c(tb, '\b');
    tb_put_c(tb, 'c');
    tb_put_c(tb, '\b');

    TEST_EQUAL_UINT('c', TB_CELL(tb, tb->rows - 1, 0).c);

    tb_put_c(tb, '\b');
    tb_put_c(tb, 'd');
    TEST_EQUAL_UINT('d', TB_CELL(tb, tb->rows - 2, tb->cols - 1).c);

    TEST_SUCCEED();
}

static bool test_simple_cursor_movements(void) {
    // We'll just test cursor value directly.

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
                tb_put_s(tb, ansi_cursor_up_n(buf, amt));
                new_row = amt > row ? 0 : row - amt;
            } else { // Down
                tb_put_s(tb, ansi_cursor_down_n(buf, amt));
                new_row = amt + row >= tb->rows ? (size_t)(tb->rows - 1) : amt + row;
            }
        } else {
            if (next_rand_u1(&r)) { // Left
                tb_put_s(tb, ansi_cursor_left_n(buf, amt));
                new_col = amt > col ? 0 : col - amt;
            } else { // Right
                tb_put_s(tb, ansi_cursor_right_n(buf, amt));
                new_col = amt + col >= tb->cols ? (size_t)(tb->cols - 1) : amt + col;
            }
        }

        TEST_EQUAL_UINT(new_row, tb->cursor_row);
        TEST_EQUAL_UINT(new_col, tb->cursor_col);

        row = new_row;
        col = new_col;
    }

    TEST_SUCCEED();
}

static bool test_next_and_prev_line(void) {
    TEST_TRUE(tb->rows >= 4);

    // Put a character in every row but the last.
    for (size_t row = 0; row < (size_t)(tb->rows - 1); row++) {
        tb_put_c(tb, 'a' + (row % 26));
        tb_put_s(tb, "\n"); // Quick put s cntl character test.
    }

    char buf[16];

    // We want to test scroll.
    tb_put_s(tb, ansi_previous_line_n(buf, tb->rows));

    // Now there is one new line on the top.

    TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, 0).c);

    for (size_t row = 1; row < tb->rows; row++) {
        TEST_EQUAL_HEX('a' + ((row - 1) % 26), (uint8_t)(TB_CELL(tb, row, 0).c));
    }

    tb_put_s(tb, ansi_previous_line_n(buf, 2));
    
    // Now there are 3 new lines at the top. (2 cut off from the bottom)

    tb_put_s(tb, ansi_next_line_n(buf, 3));
    TEST_EQUAL_HEX('a', (uint8_t)(TB_CELL(tb, 3, 0).c));

    tb_put_s(tb, ansi_next_line_n(buf, tb->rows - 3));

    // Cursor is at a bottom empty line.

    TEST_EQUAL_HEX(' ', (uint8_t)(TB_CELL(tb, tb->rows - 1, 0).c));
    TEST_EQUAL_HEX('a' + ((tb->rows - 4) % 26), (uint8_t)(TB_CELL(tb, tb->rows - 2, 0).c));

    TEST_SUCCEED();
}

static bool test_clear_options(void) {
    tb_put_s(tb, "Hello\r" ANSI_CLEAR_TO_BOL);
    TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, 0).c);
    TEST_EQUAL_HEX('e', TB_CELL(tb, 0, 1).c);

    tb_put_s(tb, ANSI_CURSOR_RIGHT ANSI_CURSOR_RIGHT ANSI_CLEAR_TO_EOL);
    TEST_EQUAL_HEX('e', TB_CELL(tb, 0, 1).c);
    TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, 2).c);
    TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, 3).c);

    tb_put_s(tb, "aye" ANSI_CLEAR_LINE);
    for (size_t i = 0; i < tb->cols; i++) {
        TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, i).c);
    }

    tb_put_s(tb, "WOO\nAYE YO\nTOOOBO\n" ANSI_CLEAR_DISPLAY);

    for (size_t row = 0; row < tb->rows; row++) {
        for (size_t col = 0; col < tb->cols; col++) {
            TEST_EQUAL_HEX(' ', TB_CELL(tb, row, col).c);
            TEST_EQUAL_HEX(tb->default_cell.style, TB_CELL(tb, row, col).style);
        }
    }

    TEST_SUCCEED();
}

static bool test_set_cursor_position(void) {
    char buf[16];

    for (size_t col = 1; col <= tb->cols; col++) { // 1-indexed loop
        tb_put_s(tb, ansi_set_cursor_col(buf, col)); 
        TEST_EQUAL_HEX(col - 1, tb->cursor_col);
    }

    tb_put_s(tb, ansi_set_cursor_col(buf, tb->cols * 2));
    TEST_EQUAL_HEX(tb->cols - 1, tb->cursor_col);

    // Now test set cursor position... RANDOM!

    tb_put_c(tb, '\r');

    rand_t r = rand(0);

    for (size_t iter = 0; iter < 100; iter++) {
        size_t new_row = next_rand_u8(&r) % (tb->rows + 10);
        size_t new_col = next_rand_u8(&r) % (tb->cols + 10);

        tb_put_s(tb, ansi_set_cursor_position(buf, new_row + 1, new_col + 1));

        if (new_row >= tb->rows) {
            new_row = tb->rows - 1;
        }

        if (new_col >= tb->cols) {
            new_col = tb->cols - 1;
        }

        TEST_EQUAL_HEX(new_row, tb->cursor_row);
        TEST_EQUAL_HEX(new_col, tb->cursor_col);
    }

    TEST_SUCCEED();
}

static bool test_scroll_up_and_down(void) {
    // The behavior of scrolling as been tested in many ways already.
    // This is really just testing that the scroll escape sequence is interpreted correctly.

    TEST_TRUE(tb->rows >= 6);

    tb_put_s(tb, "A\r");

    char buf[16];

    tb_put_s(tb, ansi_scroll_down_n(buf, 5));
    TEST_EQUAL_HEX('A', TB_CELL(tb, 5, 0).c);

    tb_put_s(tb, ansi_scroll_up_n(buf, 2));
    TEST_EQUAL_HEX('A', TB_CELL(tb, 3, 0).c);

    TEST_SUCCEED();
}

static bool test_styles(void) {
    tb_put_s(tb, ANSI_BLUE_FG ANSI_BLACK_BG "A" ANSI_ITALIC "B" ANSI_RED_BG "C" 
            ANSI_GREEN_FG "D" ANSI_RESET "E");

    const term_cell_t exp_pairs[] = {
        {term_style(TC_BLUE, TC_BLACK), 'A'},
        {TS_ITALIC | term_style(TC_BLUE, TC_BLACK), 'B'},
        {TS_ITALIC | term_style(TC_BLUE, TC_RED), 'C'},
        {TS_ITALIC | term_style(TC_GREEN, TC_RED), 'D'},
        {tb->default_cell.style, 'E'},
    };
    const size_t num_exp_pairs = sizeof(exp_pairs) / sizeof(exp_pairs[0]);

    for (size_t i = 0; i < num_exp_pairs; i++) {
        TEST_EQUAL_HEX(exp_pairs[i].style, TB_CELL(tb, 0, i).style);
        TEST_EQUAL_HEX(exp_pairs[i].c, TB_CELL(tb, 0, i).c);
    }

    TEST_SUCCEED();
}

static bool test_bad_sequences(void) {
    tb_put_s(tb, ANSI_CSI);
    TEST_EQUAL_HEX(' ', TB_CELL(tb, 0, 0).c);

    tb_put_s(tb, "\r" ANSI_CSI "$@");
    TEST_EQUAL_HEX('@', TB_CELL(tb, 0, 0).c);

    TEST_SUCCEED();
}

bool test_term_buffer(void) {
    BEGIN_SUITE("Terminal Buffer");
    RUN_TEST(test_tb_clear);
    RUN_TEST(test_tb_resize);
    RUN_TEST(test_scroll);
    RUN_TEST(test_term_put_c0);
    RUN_TEST(test_term_put_c1);
    RUN_TEST(test_simple_cursor_movements);
    RUN_TEST(test_next_and_prev_line);
    RUN_TEST(test_clear_options);
    RUN_TEST(test_set_cursor_position);
    RUN_TEST(test_scroll_up_and_down);
    RUN_TEST(test_styles);
    RUN_TEST(test_bad_sequences);
    return END_SUITE();
}
