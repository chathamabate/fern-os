
#pragma once

#include "s_util/str.h"

/*
 * FernOS Terminals will likely only support the following ANSI control sequences!
 *
 * NOTE: VERY IMPORTANT
 *
 * ANSI codes usually support default arguments. 
 * FernOS will not support this feature.
 *
 * For example, `CSI n A` move the cursor up `n` positions. 
 * Normally, if you don't provide `n` (`CSI A`), the cursor will moved up just 1 position.
 * (1 being the default argument value).
 * I prefer always explicitly giving arguments to the special ANSI commands, plus, it's 
 * easier to implement.
 */

#define ANSI_CSI "\x1B["

/*
 * ANSI Movement sequences:
 */

/*
 * Up, Down, Left, and Right commands do NOT scroll, if at the edge of the
 * display in any direction, the command does nothing.
 */

#define ANSI_CURSOR_UP        ANSI_CSI "1A"
#define ANSI_CURSOR_DOWN      ANSI_CSI "1B"
#define ANSI_CURSOR_RIGHT     ANSI_CSI "1C"
#define ANSI_CURSOR_LEFT      ANSI_CSI "1D"

static inline char *ansi_cursor_up_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dA", n);
    return buf;
}

static inline char *ansi_cursor_down_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dB", n);
    return buf;
}

static inline char *ansi_cursor_right_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dC", n);
    return buf;
}

static inline char *ansi_cursor_left_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dD", n);
    return buf;
}

/*
 * Next and previous line command DO scroll if necessary.
 *
 * NOTE: The cursor should go to the beginning of its new line.
 */

#define ANSI_CURSOR_NEXT_LINE       ANSI_CSI "1E"
#define ANSI_CURSOR_PREVIOUS_LINE   ANSI_CSI "1F"

static inline char *ansi_next_line_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dE", n);
    return buf;
}

static inline char *ansi_previous_line_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dF", n);
    return buf;
}

/**
 * Clear the entire screen and moves cursor to the upper left.
 */
#define ANSI_CLEAR_DISPLAY     ANSI_CSI "2J"

/**
 * Clear from the cursor to the end of the line.
 */
#define ANSI_CLEAR_TO_EOL      ANSI_CSI "0K"

/**
 * Clear the cursor to the beginning of the line.
 */
#define ANSI_CLEAR_TO_BOL      ANSI_CSI "1K"

/**
 * Clear the full line, cursor stays in the same position.
 */
#define ANSI_CLEAR_LINE        ANSI_CSI "2K"

/*
 * NOTE: ANSI specifies all cursor positions to be 1-based for some reason?
 */

/**
 * `col` is 1-based.
 */
static inline char *ansi_set_cursor_col(char *buf, size_t col) {
    str_fmt(buf, ANSI_CSI "%dG", col);
    return buf;
}

/**
 * `row` and `col` are both 1-based.
 */
static inline char *ansi_set_cursor_position(char *buf, size_t row, size_t col) {
    str_fmt(buf, ANSI_CSI "%d;%dH", row, col);
    return buf;
}

/*
 * Scroll up moves all lines and the cursor up, new lines are added to the bottom of the screen.
 * Scroll down is the opposite.
 *
 * The cursor is NOT moved!
 */

#define ANSI_SCROLL_UP      ANSI_CSI "1S"
#define ANSI_SCROLL_DOWN    ANSI_CSI "1T"

static inline char *ansi_scroll_up_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dS", n);
    return buf;
}

static inline char *ansi_scroll_down_n(char *buf, size_t n) {
    str_fmt(buf, ANSI_CSI "%dT", n);
    return buf;
}

/*
 * ANSI Color escape sequences:
 */

#define ANSI_RESET         ANSI_CSI "0m"

#define ANSI_BOLD          ANSI_CSI "1m"
#define ANSI_ITALIC        ANSI_CSI "3m"

#define ANSI_BLACK_FG      ANSI_CSI "30m"
#define ANSI_BLUE_FG       ANSI_CSI "31m"
#define ANSI_GREEN_FG      ANSI_CSI "32m"
#define ANSI_CYAN_FG       ANSI_CSI "33m"
#define ANSI_RED_FG        ANSI_CSI "34m"
#define ANSI_MAGENTA_FG    ANSI_CSI "35m"
#define ANSI_BROWN_FG      ANSI_CSI "36m"
#define ANSI_LIGHT_GREY_FG ANSI_CSI "37m"

#define ANSI_BRIGHT_BLACK_FG      ANSI_CSI "90m"
#define ANSI_BRIGHT_BLUE_FG       ANSI_CSI "91m"
#define ANSI_BRIGHT_GREEN_FG      ANSI_CSI "92m"
#define ANSI_BRIGHT_CYAN_FG       ANSI_CSI "93m"
#define ANSI_BRIGHT_RED_FG        ANSI_CSI "94m"
#define ANSI_BRIGHT_MAGENTA_FG    ANSI_CSI "95m"
#define ANSI_BRIGHT_BROWN_FG      ANSI_CSI "96m"
#define ANSI_BRIGHT_LIGHT_GREY_FG ANSI_CSI "97m"

#define ANSI_BLACK_BG      ANSI_CSI "40m"
#define ANSI_BLUE_BG       ANSI_CSI "41m"
#define ANSI_GREEN_BG      ANSI_CSI "42m"
#define ANSI_CYAN_BG       ANSI_CSI "43m"
#define ANSI_RED_BG        ANSI_CSI "44m"
#define ANSI_MAGENTA_BG    ANSI_CSI "45m"
#define ANSI_BROWN_BG      ANSI_CSI "46m"
#define ANSI_LIGHT_GREY_BG ANSI_CSI "47m"

#define ANSI_BRIGHT_BLACK_BG      ANSI_CSI "100m"
#define ANSI_BRIGHT_BLUE_BG       ANSI_CSI "101m"
#define ANSI_BRIGHT_GREEN_BG      ANSI_CSI "102m"
#define ANSI_BRIGHT_CYAN_BG       ANSI_CSI "103m"
#define ANSI_BRIGHT_RED_BG        ANSI_CSI "104m"
#define ANSI_BRIGHT_MAGENTA_BG    ANSI_CSI "105m"
#define ANSI_BRIGHT_BROWN_BG      ANSI_CSI "106m"
#define ANSI_BRIGHT_LIGHT_GREY_BG ANSI_CSI "107m"

