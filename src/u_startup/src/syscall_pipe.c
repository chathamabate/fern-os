
#include "u_startup/syscall_pipe.h"
#include "u_startup/syscall.h"

fernos_error_t sc_pipe_open(handle_t *h, size_t len) {
    return sc_plg_cmd(PLG_PIPE_ID, PLG_PIPE_PCID_OPEN, (uint32_t)h, (uint32_t)len, 0, 0);
}
