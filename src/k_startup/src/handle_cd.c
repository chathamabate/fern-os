
#include "k_startup/handle_cd.h"
#include "s_mem/allocator.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/page_helpers.h"

// DELETE ME SOON

#define ANSI_CSI_LOOK_BACK (32U)

#if HANDLE_CD_TX_MAX_LEN <= ANSI_CSI_LOOK_BACK
#error "TX Max Len must be greater than the lookback amount!"
#endif

static fernos_error_t copy_handle_cd_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_handle_cd_state(handle_state_t *hs);
static fernos_error_t hs_cd_wait_write_ready(handle_state_t *hs);
static fernos_error_t hs_cd_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
static fernos_error_t hs_cd_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3);

static const handle_state_impl_t HS_CD_IMPL = {
    .copy_handle_state = copy_handle_cd_state,
    .delete_handle_state = delete_handle_cd_state,
    .hs_wait_write_ready = hs_cd_wait_write_ready,
    .hs_write = hs_cd_write,
    .hs_wait_read_ready = NULL,
    .hs_read = NULL,
    .hs_cmd = hs_cd_cmd, 
};

handle_state_t *new_handle_cd_state(kernel_state_t *ks, process_t *proc, handle_t h, char_display_t *cd) {
    handle_cd_state_t *hs_cd = al_malloc(ks->al, sizeof(handle_cd_state_t));
    if (!hs_cd) {
        return NULL;
    }

    init_base_handle((handle_state_t *)hs_cd, &HS_CD_IMPL, ks, proc, h, true);

    *(char_display_t **)&(hs_cd->cd) = cd;

    return (handle_state_t *)hs_cd;
}

static fernos_error_t copy_handle_cd_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    handle_cd_state_t *hs_cd = (handle_cd_state_t *)hs;

    handle_state_t *hs_copy = new_handle_cd_state(hs_cd->super.ks, proc, hs_cd->super.handle, hs_cd->cd);
    if (!hs_copy) {
        return FOS_E_NO_MEM;
    }

    *out = hs_copy;
    return FOS_E_SUCCESS;
}

static fernos_error_t delete_handle_cd_state(handle_state_t *hs) {
    al_free(hs->ks->al, hs);
    return FOS_E_SUCCESS;
}

static fernos_error_t hs_cd_wait_write_ready(handle_state_t *hs) {
    thread_t *thr = (thread_t *)(hs->ks->schedule.head);
    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS); // A character display handle can ALWAYs 
                                                 // accept data!
}

/**
 * Writing to a Character display expects a string without a null terminator.
 *
 * `len` should be equal to `str_len(string to print)`.
 *
 * Undefined behavior if the string to print contains non-supported control characters or
 * sequences.
 */
static fernos_error_t hs_cd_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    fernos_error_t err;
    handle_cd_state_t *hs_cd = (handle_cd_state_t *)hs;

    thread_t *thr = (thread_t *)(hs->ks->schedule.head);

    if (!u_src || len == 0) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    size_t amt_to_copy = len;
    if (len > HANDLE_CD_TX_MAX_LEN) {
        amt_to_copy = HANDLE_CD_TX_MAX_LEN;
    }

    char buf[HANDLE_CD_TX_MAX_LEN + 1];

    err = mem_cpy_from_user(buf, thr->proc->pd, u_src, amt_to_copy, NULL);
    DUAL_RET_FOS_ERR(err, thr);

    buf[amt_to_copy] = '\0';

    size_t amt_to_print = amt_to_copy;
    if (amt_to_print < len) {
        // If the given buffer was cut off, i.e. we will not be printing everything here in one go,
        // there is a small chance that the final few characters in the buffer are an incomplete
        // ansi control sequence. Let's check the final 32 characters.
        
        // This loop will only work when HANDLE_CD_TX_MAX_LEN > 32. Which should always be the case.
        for (size_t i = amt_to_print - 1; i >= amt_to_print - ANSI_CSI_LOOK_BACK; i--) {
            // We found the start of a control sequence, cut off here and break the loop.
            if (buf[i] == '\x1B') {
                buf[i] = '\0';
                amt_to_print = i;
                break;
            }
        }
    }

    if (u_written) {
        err = mem_cpy_to_user(thr->proc->pd, u_written, &amt_to_print, sizeof(size_t), NULL);
        DUAL_RET_FOS_ERR(err, thr);
    }

    // success!
    cd_put_s(hs_cd->cd, buf);

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t hs_cd_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;
    handle_cd_state_t *hs_cd = (handle_cd_state_t *)hs;

    (void)arg2;
    (void)arg3;

    thread_t *thr = (thread_t *)(hs->ks->schedule.head);

    switch (cmd) {

    /*
     * Get the dimensions of the character display!
     */
    case CD_HCID_GET_DIMS: {
        size_t *u_rows = (size_t *)arg0; 
        if (u_rows) {
            err = mem_cpy_to_user(thr->proc->pd, u_rows, &(hs_cd->cd->rows), sizeof(size_t), NULL);
            DUAL_RET_FOS_ERR(err, thr);
        }

        size_t *u_cols = (size_t *)arg1;
        if (u_cols) {
            err = mem_cpy_to_user(thr->proc->pd, u_cols, &(hs_cd->cd->cols), sizeof(size_t), NULL);
            DUAL_RET_FOS_ERR(err, thr);
        }

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS); 
    }

    }
}

