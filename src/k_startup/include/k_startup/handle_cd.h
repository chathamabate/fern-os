
#pragma once

#include "state.h"
#include "handle.h"
#include "s_util/char_display.h"

/**
 * The max number of characters which can be written to a character 
 * display from userspace in one go.
 */
#define HANDLE_CD_TX_MAX_LEN (2048U)

typedef struct _handle_cd_state_t handle_cd_state_t;

/**
 * This is a generic handle state which will be compatible with any character
 * display!
 */
struct _handle_cd_state_t {
    handle_state_t super;

    char_display_t * const cd;
};

/**
 * Create a new character display handle under ownership of the given process with
 * the given handle `h`.
 *
 * This uses the allocator found in `ks`.
 */
handle_state_t *new_handle_cd_state(kernel_state_t *ks, process_t *proc, handle_t h, char_display_t *cd);
