

#include "u_startup/syscall.h"
#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/shared_defs.h"
#include "s_util/str.h"
#include "s_util/ansi.h"
#include <stddef.h>

/**
 * This is a test application which enters different paths depending on the
 * arguments given. Each path is meant to correspond to a unique case!
 */

proc_exit_status_t app_main(const char * const *args, size_t num_args) {
    if (num_args == 0) {
        return 100; // Here we confirm proc exit status works as expected.
    }

    switch (args[0][0]) {
    case 'a': // Run forever.
        while (1);

    default:
        return PROC_ES_SUCCESS;
    }
}
