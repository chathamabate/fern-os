
#include "u_startup/syscall.h"
#include "u_startup/syscall_term.h"

void sc_term_get_dimmensions(handle_t h, size_t *rows, size_t *cols) {
    (void)sc_handle_cmd(h, TERM_HCID_GET_DIMS, (uint32_t)rows, (uint32_t)cols, 0, 0);
}
