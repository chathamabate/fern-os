

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

    fernos_error_t err;

    switch (args[0][0]) {
    case 'a': // Run forever.
        while (1);

    case 'b': // Loop forever reading from Default IN and outputing to Default OUT.
        while (true) {
            err = sc_in_wait();
            if (err != FOS_E_SUCCESS) {
                return PROC_ES_SUCCESS;
            }

            size_t readden;
            char buf[100];
            while ((err = sc_in_read(buf, sizeof(buf), &readden)) == FOS_E_SUCCESS) {
                for (size_t i = 0; i < readden; i++) {
                    buf[i]++; // Increment each character by 1 before writing back out.
                }

                err = sc_out_write_full(buf, readden);
                if (err != FOS_E_SUCCESS) {
                    return PROC_ES_FAILURE;
                }
            }

            if (err != FOS_E_EMPTY) {
                return PROC_ES_FAILURE;
            }
        }

    default:
        return PROC_ES_SUCCESS;
    }
}
