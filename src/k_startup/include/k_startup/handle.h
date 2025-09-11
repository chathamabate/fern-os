
#pragma once

#include "s_util/err.h"
#include "k_startup/fwd_defs.h"

/*
 * A "Handle" will be the new generic way of reading/writing from 
 * userspace to the kernel.
 *
 * This will slightly mimic linux character devices. The intention
 * being that one interface can be used for speaking with a variety
 * of different things! (Files, hardware devices, pipes, etc...)
 */

typedef struct _handle_state_impl_t {
    fernos_error_t (*hs_write)(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
    fernos_error_t (*hs_read)(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
    fernos_error_t (*hs_cmd)(handle_state_t *hs, handle_cmd_t );
} handle_state_impl_t;

struct _handle_state_t {

};

