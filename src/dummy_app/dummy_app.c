

#include "u_startup/syscall.h"
#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/shared_defs.h"
#include "s_util/str.h"
#include "s_util/ansi.h"
#include <stddef.h>

/*
 * This is a dummy application which should confirm ELF loading works as expected.
 *
 * It takes in arguments and copies them into dynamic buffers on the heap.
 * Each argument has their lowercase letters capitalized in the dynamic copies.
 * Each copy is then printed to the terminal.
 */

/**
 * This should be placed in .bss and zero'd
 */
const char *cap_args[10];

/**
 * `prefix` should live in .data
 * The literal should live in .text
 */
const char *prefix = ANSI_BRIGHT_GREEN_FG "> " ANSI_RESET;


fernos_error_t app_main(const char * const *args, size_t num_args) {
    fernos_error_t err;

    // For now, this will use `sc_out_write_s` for output.
    // Later, if I change how process output works, this may also change.

    // On load, global fields should be reset to their very first values.
    // In the case of the default allocator, this should be NULL.
    if (get_default_allocator()) {
        sc_out_write_s("Allocator is already initialized\n");
        return PROC_ES_FAILURE;
    }

    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        sc_out_write_s("Failure to setup simple heap\n");
        return PROC_ES_FAILURE;
    }

    // Since `cap_args` is in .bss, it should be set to 0 on load!
    const size_t cap_args_len = sizeof(cap_args) / sizeof(cap_args[0]);
    for (size_t i = 0; i < cap_args_len; i++) {
        if (cap_args[i]) {
            sc_out_write_s(".bss is not zero'd\n");
            return PROC_ES_FAILURE;
        }
    }

    const size_t iters = cap_args_len < num_args ? cap_args_len : num_args;
    for (size_t i = 0; i < iters; i++) {
        const char *arg = args[i];
        const size_t sl = str_len(arg);

        char *cap_arg = da_malloc(sl + 1);
        if (!cap_arg) {
            sc_out_write_s("Failed to allocate capital string buffer\n");
            return PROC_ES_FAILURE;
        }

        mem_cpy(cap_arg, arg, sl + 1);

        for (size_t j = 0; j < sl; j++) {
            char c = cap_arg[j];
            if ('a' <= c && c <= 'z') {
                cap_arg[j] += 'A' - 'a';
            }
        }

        cap_args[i] = cap_arg;
    }

    for (size_t i = 0; i < iters; i++) {
        sc_out_write_s(prefix); 
        sc_out_write_s(cap_args[i]);
        sc_out_write_s("\n");
    }

    return PROC_ES_SUCCESS;
}
