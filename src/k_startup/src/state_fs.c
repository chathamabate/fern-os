
#include "k_startup/state_fs.h"

#include "k_startup/fwd_defs.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/thread.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "s_block_device/file_sys.h"

fernos_error_t ks_fs_register_nk(kernel_state_t *ks, fs_node_key_t nk, fs_node_key_t *kernel_nk) {
    fernos_error_t err;

    if (!nk) {
        return FOS_BAD_ARGS;
    }

    const fs_node_key_t *key_p;
    kernel_fs_node_state_t **node_state_p;

    err = mp_get_kvp(ks->nk_map, &nk, (const void **)&key_p, (void **)&node_state_p);
    if (err != FOS_SUCCESS && err != FOS_EMPTY) {
        return err;
    }

    fs_node_key_t key;
    kernel_fs_node_state_t *node_state;

    if (err == FOS_EMPTY) { // New entry!
        fs_node_info_t info;
        err = fs_get_node_info(ks->fs, nk, &info);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        fs_node_key_t nk_copy = fs_new_key_copy(ks->fs, nk);
        node_state = al_malloc(ks->al, sizeof(kernel_fs_node_state_t));
        timed_wait_queue_t *twq = info.is_dir ? NULL : new_timed_wait_queue(ks->al);

        if (nk_copy && node_state && (info.is_dir || twq)) {
            node_state->twq = twq;
            node_state->references = 1;

            err = mp_put(ks->nk_map, &nk_copy, &node_state);
        }

        if (!nk_copy || !node_state || (!(info.is_dir) && !twq) || err != FOS_SUCCESS) {
            if (nk_copy) {
                mp_remove(ks->nk_map, &nk_copy);
            }

            delete_wait_queue((wait_queue_t *)twq);
            fs_delete_key(ks->fs, nk_copy);
            al_free(ks->al, node_state);

            return FOS_NO_MEM;
        }

        key = nk_copy;
    } else {
        key = *key_p;
        node_state = *node_state_p;

        if (!key || !node_state) {
            return FOS_STATE_MISMATCH; // Something very wrong here if this happens.
        }

        node_state->references++;
    }

    if (kernel_nk) {
        *kernel_nk = key;
    }

    return FOS_SUCCESS;
}

fernos_error_t ks_fs_deregister_nk(kernel_state_t *ks, fs_node_key_t nk) {
    if (!nk) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    fs_node_key_t *key_p;
    kernel_fs_node_state_t **state_p;

    err = mp_get_kvp(ks->nk_map, &nk, (const void **)&key_p, (void **)&state_p);
    if (err != FOS_SUCCESS) {
        return FOS_STATE_MISMATCH;
    }

    fs_node_key_t key = *key_p;
    kernel_fs_node_state_t *state = *state_p;

    if (!key || !state) {
        return FOS_STATE_MISMATCH;
    }

    if (--(state->references) == 0) {
        // Cleanup time!

        // First remove our entry from the nk map.
        mp_remove(ks->nk_map, &key);

        // Also delete the mapped key, (NOT THE KEY INPUT TO THIS FUNCTION)
        fs_delete_key(ks->fs, key);

        // Now deal with the leftover state.

        if (state->twq) {
            // The way we are using the timed queue here, this should ALWAYS wake up
            // all waiting threads.
            twq_notify(state->twq, UINT32_MAX);

            thread_t *woken_thread;
            while ((err = twq_pop(state->twq, (void **)&woken_thread)) == FOS_SUCCESS) {
                woken_thread->wq = NULL;
                woken_thread->ctx.eax = FOS_STATE_MISMATCH;
                woken_thread->state = THREAD_STATE_DETATCHED;

                ks_schedule_thread(ks, woken_thread);
            }

            if (err != FOS_EMPTY) {
                return FOS_UNKNWON_ERROR;
            }

            delete_wait_queue((wait_queue_t *)(state->twq));
        }

        al_free(ks->al, state);
    }

    return FOS_SUCCESS;
}

fernos_error_t ks_fs_deregister_proc_nks(kernel_state_t *ks, process_t *proc) {
    fernos_error_t err;

    idtb_reset_iterator(proc->file_handle_table);
    for (file_handle_t fh = idtb_get_iter(proc->file_handle_table);
            fh != idtb_null_id(proc->file_handle_table); 
            fh = idtb_next(proc->file_handle_table)) {
        file_handle_state_t *fh_state = idtb_get(proc->file_handle_table, fh);
        if (!fh_state) {
            return FOS_STATE_MISMATCH; // If this happens something is very wrong.
        }

        err = ks_fs_deregister_nk(ks, fh_state->nk);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    err = ks_fs_deregister_nk(ks, proc->cwd);
    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t ks_fs_set_wd(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    process_t *proc = ks->curr_thread->proc;

    // Path length is too long or not given.
    DUAL_RET_COND(
        !u_path || u_path_len > FS_MAX_PATH_LEN, ks->curr_thread,
        FOS_BAD_ARGS, FOS_SUCCESS
    );

    char path[FS_MAX_PATH_LEN + 1];

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len + 1, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    // Get a node key for the given path.
    // Confirm that it points to a directory!

    fs_node_key_t nk;

    err = fs_new_key(ks->fs, proc->cwd, path, &nk);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    fs_node_info_t info;
    err = fs_get_node_info(ks->fs, nk, &info);
    if (err != FOS_SUCCESS || !(info.is_dir)) {
        fs_delete_key(ks->fs, nk);        
        DUAL_RET(ks->curr_thread, err != FOS_SUCCESS ? err : FOS_STATE_MISMATCH, FOS_SUCCESS);
    }

    // Ok we have a node key which *could* become a valid cwd.
    // Let's attempt to register it with the kernel, then, replace the old cwd with this one.

    fs_node_key_t kernel_nk;

    err = ks_fs_register_nk(ks, nk, &kernel_nk);

    // No matter what happens, we don't need the original nk anymore.
    fs_delete_key(ks->fs, nk);

    if (err != FOS_SUCCESS) {
        DUAL_RET(ks->curr_thread, FOS_UNKNWON_ERROR, FOS_SUCCESS); 
    }

    err = ks_fs_deregister_nk(ks, proc->cwd);
    if (err != FOS_SUCCESS) {
        return err; // Deregister only fails in the case of some fatal kernel problem.
    }

    proc->cwd = kernel_nk;

    return FOS_SUCCESS; 
}

fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len, file_handle_t *u_fh) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    process_t *proc = ks->curr_thread->proc;

    // Path length is too long or not given.
    DUAL_RET_COND(
        !u_path || u_path_len > FS_MAX_PATH_LEN, ks->curr_thread,
        FOS_BAD_ARGS, FOS_SUCCESS
    );

    char path[FS_MAX_PATH_LEN + 1];

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len + 1, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    // And we gotta deal with making file handles... agh... I can barely think.
    err = fs_touch();

    return FOS_SUCCESS;
}

fernos_error_t ks_fs_mkdir(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_remove(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_get_info(kernel_state_t *ks, const char *u_path, size_t u_path_len, fs_node_info_t *u_info) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_get_child_name(kernel_state_t *ks, const char *u_path, size_t u_path_len, size_t index, char *u_child_name) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_open(kernel_state_t *ks, char *u_path, size_t u_path_len, file_handle_t *u_fh) {
    return FOS_NOT_IMPLEMENTED;
}

void ks_fs_close(kernel_state_t *ks, file_handle_t fh) {
}

fernos_error_t ks_fs_seek(kernel_state_t *ks, file_handle_t fh, size_t pos) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_write(kernel_state_t *ks, file_handle_t fh, const void *u_src, size_t len, size_t *u_written) {
    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_read(kernel_state_t *ks, file_handle_t fh, void *u_dst, size_t len, size_t *u_readden) {
    return FOS_NOT_IMPLEMENTED;
}

