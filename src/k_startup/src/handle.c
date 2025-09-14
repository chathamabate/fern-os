
#include "k_startup/handle.h"

fernos_error_t clear_handle_table(id_table_t *handle_table) {
    fernos_error_t err;
    for (size_t h = 0; h < FOS_MAX_HANDLES_PER_PROC; h++) {
        err = delete_handle_state(idtb_get(handle_table, h));
        if (err != FOS_SUCCESS) {
            return FOS_ABORT_SYSTEM;
        }
        idtb_push_id(handle_table, h);
    }
    return FOS_SUCCESS;
}

// DELETE ME
#include "k_bios_term/term.h"

fernos_error_t copy_handle_table(process_t *parent, process_t *child) {
    fernos_error_t err = FOS_SUCCESS;

    const handle_t NULL_HANDLE = idtb_null_id(parent->handle_table);

    idtb_reset_iterator(parent->handle_table);
    for (handle_t h = idtb_get_iter(parent->handle_table);
            h != NULL_HANDLE && err == FOS_SUCCESS; 
            h = idtb_next(parent->handle_table)) {
        handle_state_t *hs = idtb_get(parent->handle_table, h);
        if (!hs) {
            return FOS_ABORT_SYSTEM; // Should never happen!
        }

        handle_state_t *hs_copy;
        err = copy_handle_state(hs, child, &hs_copy);
        if (err == FOS_ABORT_SYSTEM) {
            return FOS_ABORT_SYSTEM;
        }

        if (err == FOS_SUCCESS) {
            err = idtb_request_id(child->handle_table, hs->handle);
        } 

        if (err == FOS_SUCCESS) {
            idtb_set(child->handle_table, hs->handle, hs_copy);
        }
    }

    // This is the case where some not catastrophic error occured,
    // let's just go through the child's handle table and delete all handles which were 
    // successfully copied.
    if (err != FOS_SUCCESS) { 
        term_put_fmt_s("FORK ERROR: 0x%X\n", err);

        for (id_t i = 0; i < FOS_MAX_HANDLES_PER_PROC; i++) {
            err = delete_handle_state(idtb_get(child->handle_table, i));
            if (err != FOS_SUCCESS) {
                return FOS_ABORT_SYSTEM; // Failing to delete a handle state is always
                                         // catastrophic.
            }

            idtb_push_id(child->handle_table, i);
        }

        return FOS_UNKNWON_ERROR;
    }

    return FOS_SUCCESS;
}
