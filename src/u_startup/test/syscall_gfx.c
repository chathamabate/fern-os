#include "u_startup/test/syscall_gfx.h"
#include "u_startup/syscall_gfx.h"
#include "u_startup/syscall.h"

void test_gfx_simple(void) {
    fernos_error_t err; 

    sc_out_write_fmt_s("Creating new graphics window!\n");

    handle_t h;
    gfx_color_t *banks[2];
    err = sc_gfx_new_gfx_window(&h, &banks);
    if (err != FOS_E_SUCCESS) {
        sc_out_write_fmt_s("Failed to create gfx window: 0x%X\n", err);
        return;
    }



    while (1);
}
