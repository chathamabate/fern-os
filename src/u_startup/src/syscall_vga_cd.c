
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/syscall.h"

fernos_error_t sc_vga_cd_open(handle_t *h) {
    return sc_plg_cmd(PLG_VGA_CD_ID, PLG_VGA_CD_PCID_OPEN, (uint32_t)h, 0, 0, 0);
}
