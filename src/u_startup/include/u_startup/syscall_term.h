
#pragma once

#include "s_bridge/shared_defs.h"

/**
 * Get the rows and columns of a handle which outputs to a terminal.
 *
 * Both `rows` and `cols` are optional.
 */
void sc_term_get_dimmensions(handle_t h, size_t *rows, size_t *cols);
