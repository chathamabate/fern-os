

#include "s_util/constraints.h"
#include "k_startup/state.h"

/**
 * Creates a new kernel state with basically no fleshed out details.
 */
kernel_state_t *new_blank_kernel_state(allocator_t *al) {
    kernel_state_t *ks = al_malloc(al, sizeof(kernel_state_t));
    if (!ks) {
        return NULL;
    }

    id_table_t *pt = new_id_table(al, FOS_MAX_PROCS);
    if (!pt) {
        al_free(al, ks);
        return NULL;
    }

    ks->al = al;
    ks->curr_thread = NULL;
    ks->proc_table = pt;

    return ks;
}

