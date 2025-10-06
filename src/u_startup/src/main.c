
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_cd.h"
#include "u_startup/syscall_vga_cd.h"
#include "u_startup/test/syscall.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_util/constraints.h"

#include "s_util/str.h"
#include "s_elf/elf32.h"
#include <stdarg.h>

static fernos_error_t setup_user_heap(void) {
    allocator_t *al = new_simple_heap_allocator(
        (simple_heap_attrs_t) {
            .start = (void *)FOS_FREE_AREA_START,
            .end = (const void *)(FOS_FREE_AREA_END),

            .mmp = (mem_manage_pair_t) {
                .request_mem = sc_mem_request,
                .return_mem = sc_mem_return
            },

            .small_fl_cutoff = 0x100,
            .small_fl_search_amt = 0x20,
            .large_fl_search_amt = 0x200
        }
    );

    if (!al) {
        return FOS_E_UNKNWON_ERROR;
    }

    set_default_allocator(al);

    return FOS_E_SUCCESS;
}

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_set_out_handle(cd);

    err = setup_user_heap();
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_out_write_fmt_s("HDR SIZE: %X\n", sizeof(elf32_header_t));

    return PROC_ES_SUCCESS;
}
