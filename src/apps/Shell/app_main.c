

#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include <stddef.h>

proc_exit_status_t app_main(const char * const *args, size_t num_args) {
    sc_out_write_s("Hello from shell\n");
    return PROC_ES_SUCCESS;
}
