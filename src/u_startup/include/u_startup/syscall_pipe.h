
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Create a new pipe!
 *
 * `len` is the number of bytes which can be stored at one time
 * within the pipe.
 *
 * On success, the write end of the handle is written `*write_h`.
 * The read end of the handle is written to `*read_h`.
 */
fernos_error_t sc_pipe_open(handle_t *write_h, handle_t *read_h, size_t len);
