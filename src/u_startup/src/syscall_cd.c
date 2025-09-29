
#include "u_startup/syscall.h"
#include "u_startup/syscall_cd.h"

void sc_cd_get_dimmensions(handle_t h, size_t *rows, size_t *cols) {
    (void)sc_handle_cmd(h, CD_HCID_GET_DIMS, (uint32_t)rows, (uint32_t)cols, 0, 0);
}
