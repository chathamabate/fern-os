
#pragma once

#define ANSI_CSI "\x1B["

/*
 * ANSI Movement sequences:
 */

/*
 * ANSI Color escape sequences:
 */

#define ANSI_RESET         ANSI_CSI "0m"

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

