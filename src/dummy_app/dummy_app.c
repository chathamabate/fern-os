

#include "s_util/str.h"
#include "u_startup/syscall.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/shared_defs.h"
#include <stddef.h>

/*
 * This is a dummy application, it takes all given args, and copies them onto the heap as
 * capitalized. Then prints out one at a time.
 */

const char **dyna_args;

fernos_error_t app_main(const char * const *args, size_t num_args) {
    fernos_error_t err;

    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }



    return 0;
}
