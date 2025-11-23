
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Create a new pipe!
 *
 * `len` is the number of bytes which can be stored at one time
 * within the pipe.
 *
 * On success, the pipe's handle is written to `*h`.
 */
fernos_error_t sc_pipe_open(handle_t *h, size_t len);
