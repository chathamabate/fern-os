
#pragma once

#include "s_util/err.h"
#include "s_util/char_display.h"

extern char_display_t * const VGA_CD;

/**
 * VGA_CD is unusable until this function is called.
 */
fernos_error_t init_vga_char_display(void);

