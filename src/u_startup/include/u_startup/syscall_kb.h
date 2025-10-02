
#pragma once

#include "u_startup/syscall.h"

/**
 * Open a handle to the keyboard.
 *
 * Returns FOS_E_BAD_ARGS if `h` is NULL.
 */
fernos_error_t sc_kb_open(handle_t *h);

/**
 * Under the hood, the keyboard plugin holds a cyclic buffer keycodes. 
 *
 * Use this call to move `h`'s position to the front of the buffer. This is useful in the case
 * where you may not have read from `h` in a while and want to just skip all keypresses which
 * may have occured while doing other work.
 */
void sc_kb_skip_forward(handle_t h);

