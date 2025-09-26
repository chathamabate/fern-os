

#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Open the VGA Character Display.
 *
 * NOTE: The returned handle can only be written to. The handle expects
 * ascii string data (ansi escape codes allowed). Do not write the NULL
 * Terminator, this will be appended automatically.
 */
fernos_error_t sc_vga_cd_open(handle_t *h);
