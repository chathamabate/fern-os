
#include "k_startup/state_fs.h"

#include "k_startup/fwd_defs.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/thread.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "s_block_device/file_sys.h"
#include "s_util/str.h"
#include "s_util/constraints.h"

fernos_error_t ks_fs_register_nk(kernel_state_t *ks, fs_node_key_t nk, fs_node_key_t *kernel_nk) {
    fernos_error_t err;

    if (!nk) {
        return FOS_ABORT_SYSTEM; // Incorrect usage inside the kernel!
    }

    const fs_node_key_t *key_p;
    kernel_fs_node_state_t **node_state_p;

    err = mp_get_kvp(ks->nk_map, &nk, (const void **)&key_p, (void **)&node_state_p);
    if (err != FOS_SUCCESS && err != FOS_EMPTY) {
        return FOS_ABORT_SYSTEM;
    }

    fs_node_key_t key;
    kernel_fs_node_state_t *node_state;

    if (err == FOS_EMPTY) { // New entry!
        fs_node_info_t info;
        err = fs_get_node_info(ks->fs, nk, &info);
        if (err != FOS_SUCCESS) { // Maybe a disk error could happen, which is allowable!
            return FOS_UNKNWON_ERROR;
        }

        fs_node_key_t nk_copy = fs_new_key_copy(ks->fs, nk);
        node_state = al_malloc(ks->al, sizeof(kernel_fs_node_state_t));
        basic_wait_queue_t *bwq = info.is_dir ? NULL : new_basic_wait_queue(ks->al);

        if (nk_copy && node_state && (info.is_dir || bwq)) {
            node_state->bwq = bwq;
            node_state->references = 1;

            err = mp_put(ks->nk_map, &nk_copy, &node_state);
        }

        if (!nk_copy || !node_state || (!(info.is_dir) && !bwq) || err != FOS_SUCCESS) {
            if (nk_copy) {
                mp_remove(ks->nk_map, &nk_copy);
            }

            delete_wait_queue((wait_queue_t *)bwq);
            fs_delete_key(ks->fs, nk_copy);
            al_free(ks->al, node_state);

            return FOS_NO_MEM;
        }

        key = nk_copy;
    } else {
        key = *key_p;
        node_state = *node_state_p;

        if (!key || !node_state) {
            // The map is malformed if this is the case!
            return FOS_ABORT_SYSTEM;
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
        return FOS_ABORT_SYSTEM;
    }

    fernos_error_t err;

    fs_node_key_t *key_p;
    kernel_fs_node_state_t **state_p;

    err = mp_get_kvp(ks->nk_map, &nk, (const void **)&key_p, (void **)&state_p);
    if (err != FOS_SUCCESS) {
        return FOS_ABORT_SYSTEM; // The given key is expected to be in the map here!
    }

    fs_node_key_t key = *key_p;
    kernel_fs_node_state_t *state = *state_p;

    if (!key || !state) {
        return FOS_ABORT_SYSTEM;
    }

    if (--(state->references) == 0) {
        // Cleanup time!

        // First remove our entry from the nk map.
        mp_remove(ks->nk_map, &key);

        // Also delete the mapped key, (NOT THE KEY INPUT TO THIS FUNCTION)
        fs_delete_key(ks->fs, key);

        // Now deal with the leftover state.

        if (state->bwq) {
            err = bwq_notify_all(state->bwq);
            if (err != FOS_SUCCESS) {
                return FOS_ABORT_SYSTEM;
            }

            thread_t *woken_thread;
            while ((err = bwq_pop(state->bwq, (void **)&woken_thread)) == FOS_SUCCESS) {
                woken_thread->wq = NULL;
                mem_set(woken_thread->wait_ctx, 0, sizeof(woken_thread->wait_ctx));
                woken_thread->ctx.eax = FOS_STATE_MISMATCH;
                woken_thread->state = THREAD_STATE_DETATCHED;

                ks_schedule_thread(ks, woken_thread);
            }

            // When the while loop above exits, err is gauranteed to be FOS_EMPTY.
                
            delete_wait_queue((wait_queue_t *)(state->bwq));
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
            return FOS_ABORT_SYSTEM; // File handle table is malformed!
        }

        err = ks_fs_deregister_nk(ks, fh_state->nk);
        if (err != FOS_SUCCESS) {
            return FOS_ABORT_SYSTEM;
        }
    }

    err = ks_fs_deregister_nk(ks, proc->cwd);
    if (err != FOS_SUCCESS) {
        return FOS_ABORT_SYSTEM;
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

    if (err == FOS_ABORT_SYSTEM) {
        return FOS_ABORT_SYSTEM;
    }

    // Other errors are acceptable for userspace.
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    err = ks_fs_deregister_nk(ks, proc->cwd);
    if (err != FOS_SUCCESS) {
        return err; // Deregister only fails in the case of some fatal kernel problem.
    }

    proc->cwd = kernel_nk;

    DUAL_RET(ks->curr_thread, FOS_SUCCESS, FOS_SUCCESS);
}

typedef enum _ks_fs_path_io_action_t {
    KS_FS_MKDIR,
    KS_FS_TOUCH,
    KS_FS_REMOVE,
} ks_fs_path_io_action_t;

static fernos_error_t ks_fs_path_io(kernel_state_t *ks, const char *u_path, size_t u_path_len, ks_fs_path_io_action_t action) {
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

    // Used for removal.
    fs_node_key_t nk;

    switch (action) {
    case KS_FS_MKDIR:
        err = fs_mkdir_path(ks->fs, proc->cwd, path, NULL);
        break;
    case KS_FS_TOUCH:
        err = fs_touch_path(ks->fs, proc->cwd, path, NULL);
        break;
    case KS_FS_REMOVE:
        // Slight oversight when making this function. remove is slightly different than mkdir and touch.
        // We need to make sure the given key is not referenced by the kernel before deleting!

        err = fs_new_key(ks->fs, proc->cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, ks->curr_thread);

        // If a node key has an entry in the map, it MUST be referenced at least once!
        bool in_use = mp_get(ks->nk_map, &nk) != NULL;
        fs_delete_key(ks->fs, nk);

        if (in_use) {
            DUAL_RET(ks->curr_thread, FOS_IN_USE, FOS_SUCCESS);
        }

        err = fs_remove_path(ks->fs, proc->cwd, path);
        break;
    default:
        return FOS_BAD_ARGS;
    }

    DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
}

fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    return ks_fs_path_io(ks, u_path, u_path_len, KS_FS_TOUCH);
}

fernos_error_t ks_fs_mkdir(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    return ks_fs_path_io(ks, u_path, u_path_len, KS_FS_MKDIR);
}

fernos_error_t ks_fs_remove(kernel_state_t *ks, const char *u_path, size_t u_path_len) {
    return ks_fs_path_io(ks, u_path, u_path_len, KS_FS_REMOVE);
}

fernos_error_t ks_fs_get_info(kernel_state_t *ks, const char *u_path, size_t u_path_len, fs_node_info_t *u_info) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    process_t *proc = ks->curr_thread->proc;

    // Path length is too long or not given.
    DUAL_RET_COND(
        !u_path || u_path_len > FS_MAX_PATH_LEN || !u_info, 
        ks->curr_thread, FOS_BAD_ARGS, FOS_SUCCESS
    );

    char path[FS_MAX_PATH_LEN + 1];

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len + 1, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    fs_node_key_t nk;

    err = fs_new_key(ks->fs, proc->cwd, path, &nk);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    fs_node_info_t info;
    err = fs_get_node_info(ks->fs, nk, &info);

    // Delete 'nk' no matter what happens.
    fs_delete_key(ks->fs, nk);

    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    err = mem_cpy_to_user(proc->pd, u_info, &info, sizeof(fs_node_info_t), NULL);
    DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
}

fernos_error_t ks_fs_get_child_name(kernel_state_t *ks, const char *u_path, size_t u_path_len, size_t index, char *u_child_name) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    process_t *proc = ks->curr_thread->proc;

    // Path length is too long or not given.
    DUAL_RET_COND(
        !u_path || u_path_len > FS_MAX_PATH_LEN || !u_child_name, 
        ks->curr_thread, FOS_BAD_ARGS, FOS_SUCCESS
    );

    char path[FS_MAX_PATH_LEN + 1];

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len + 1, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    fs_node_key_t nk;

    err = fs_new_key(ks->fs, proc->cwd, path, &nk);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    // We are going to reuse the path buffer because what the hell.
    err = fs_get_child_names(ks->fs, nk, index, 1, (char (*)[FS_MAX_FILENAME_LEN + 1])path);

    // Delete 'nk' no matter what happens.
    fs_delete_key(ks->fs, nk);

    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    size_t name_len = str_len(path);

    err = mem_cpy_to_user(proc->pd, u_child_name, path, name_len + 1, NULL);
    DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
}

fernos_error_t ks_fs_open(kernel_state_t *ks, const char *u_path, size_t u_path_len, file_handle_t *u_fh) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;
    process_t *proc = ks->curr_thread->proc;

    // Path length is too long or not given.
    DUAL_RET_COND(
        !u_path || u_path_len > FS_MAX_PATH_LEN || !u_fh, 
        ks->curr_thread, FOS_BAD_ARGS, FOS_SUCCESS
    );

    char path[FS_MAX_PATH_LEN + 1];

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len + 1, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    const file_handle_t NULL_FH = idtb_null_id(proc->file_handle_table);
    file_handle_t fh = NULL_FH;
    file_handle_state_t *fh_state = NULL;
    fs_node_key_t nk = NULL;
    fs_node_key_t kernel_nk = NULL;
    fs_node_info_t info;

    err = fs_new_key(ks->fs, proc->cwd, path, &nk);

    if (err == FOS_SUCCESS) {
        err = ks_fs_register_nk(ks, nk, &kernel_nk);
        if (err == FOS_ABORT_SYSTEM) { // In case of non-user recoverable error!
            return err;
        }
    }

    // No matter the status, we don't need nk after this point!
    fs_delete_key(ks->fs, nk);

    if (err == FOS_SUCCESS) {
        err = fs_get_node_info(ks->fs, kernel_nk, &info);
    }

    if (err == FOS_SUCCESS && info.is_dir) {
        err = FOS_STATE_MISMATCH; 
    }

    if (err == FOS_SUCCESS) {
        fh = idtb_pop_id(proc->file_handle_table);
        if (fh == NULL_FH) {
            err = FOS_EMPTY; // We have no more space in the handle table!
        }
    }

    if (err == FOS_SUCCESS) {
        // We'll just try the copy before actually putting things into the handle table just to make clean up easier!
        err = mem_cpy_to_user(proc->pd, u_fh, &fh, sizeof(file_handle_t), NULL);
    }

    if (err == FOS_SUCCESS) {
        fh_state = al_malloc(proc->al, sizeof(file_handle_state_t));
        if (!fh_state) {
            err = FOS_NO_MEM;
        }
    } 

    if (err == FOS_SUCCESS) {
        // SUCCESS!
        *(fs_node_key_t *)&fh_state->nk = kernel_nk;
        fh_state->pos = 0;

        idtb_set(proc->file_handle_table, fh, fh_state);
    } else {
        // Otherwise.... Cleanup!!!
        
        al_free(proc->al, fh_state);
        idtb_push_id(proc->file_handle_table, fh);
        if (kernel_nk && ks_fs_deregister_nk(ks, kernel_nk) == FOS_ABORT_SYSTEM) {
            // NOTE: since we are using the very key which IS registered with the kernel,
            // we don't need to delete it. `ks_fs_deregister_nk` will do that for us.
            return FOS_ABORT_SYSTEM;
        }
    }

    DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
}

fernos_error_t ks_fs_close(kernel_state_t *ks, file_handle_t fh) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;
    process_t *proc = ks->curr_thread->proc;

    file_handle_state_t *state = idtb_get(proc->file_handle_table, fh);
    if (!state) {
        return FOS_SUCCESS; // This is OK, remember no current thread return.
    }

    // We got our state, Let's remove file handle from the id table plz.
    idtb_push_id(proc->file_handle_table, fh);

    err = ks_fs_deregister_nk(ks, state->nk);
    if (err == FOS_ABORT_SYSTEM) {
        return FOS_UNKNWON_ERROR;
    }

    // REMEMBER!!! The key pointer is shared!! We rely on ks_fs_deregister above
    // to delete it when necessary.
    al_free(proc->al, state);

    return FOS_SUCCESS;
}

fernos_error_t ks_fs_seek(kernel_state_t *ks, file_handle_t fh, size_t pos) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;
    process_t *proc = ks->curr_thread->proc;

    file_handle_state_t *state = idtb_get(proc->file_handle_table, fh);
    DUAL_RET_COND(!state, ks->curr_thread, FOS_INVALID_INDEX, FOS_SUCCESS);

    fs_node_info_t info;
    err = fs_get_node_info(ks->fs, state->nk, &info);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    if (info.is_dir) { // Quick sanity check!
        return FOS_STATE_MISMATCH;
    }

    state->pos = pos > info.len ? info.len : pos;
    DUAL_RET(ks->curr_thread, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_fs_write(kernel_state_t *ks, file_handle_t fh, const void *u_src, size_t len, size_t *u_written) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    DUAL_RET_COND(!u_src || !u_written, ks->curr_thread, FOS_BAD_ARGS, FOS_SUCCESS);

    fernos_error_t err;
    process_t *proc = ks->curr_thread->proc;

    file_handle_state_t *state = idtb_get(proc->file_handle_table, fh);
    DUAL_RET_COND(!state, ks->curr_thread, FOS_INVALID_INDEX, FOS_SUCCESS);

    if (state->pos == SIZE_MAX) { // Can exit early if there is no way the file can be expanded.
        DUAL_RET(ks->curr_thread, FOS_NO_SPACE, FOS_SUCCESS);
    }

    fs_node_info_t info;
    err = fs_get_node_info(ks->fs, state->nk, &info);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    if (info.is_dir) { // Quick sanity check!
        return FOS_STATE_MISMATCH;
    }

    const size_t old_len = info.len;

    if (state->pos > old_len) {
        // Another quick sanity check. (This should never happen!)
        return FOS_STATE_MISMATCH;
    }

    // Ok, expansion may be necessary!
    
    size_t bytes_to_write = len;
    if (state->pos + bytes_to_write < state->pos) { // check wrap.
        bytes_to_write = SIZE_MAX - state->pos;
    }

    if (bytes_to_write > KS_FS_TX_MAX_LEN) {
        bytes_to_write = KS_FS_TX_MAX_LEN;
    }

    uint8_t tx_buf[KS_FS_TX_MAX_LEN];
    err = mem_cpy_from_user(tx_buf, proc->pd, u_src, bytes_to_write, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    const size_t bytes_left = old_len - state->pos;

    if (bytes_left < bytes_to_write) { // expansion is needed.
        err = fs_resize(ks->fs, state->nk, state->pos + bytes_to_write);
        DUAL_RET_FOS_ERR(err, ks->curr_thread);
    }

    err = fs_write(ks->fs, state->nk, state->pos, bytes_to_write, tx_buf);
    
    // We are about to potentially schedule some threads, so `curr_thread` will no longer be usable reliably.
    thread_t *calling_thread = ks->curr_thread;

    // Ok, so, if we had to expand our file, that means we must wake up all our blocked reading threads!
    // We do this regardless of whether or not the above write succeeded or not.
    // This could potentially result in faulty data read in the case where the write fails.
    // But whatever.
    if (bytes_left < bytes_to_write) {
        const size_t bytes_appended = bytes_to_write - bytes_left;

        fernos_error_t tmp_err;

        kernel_fs_node_state_t **node_state_p = mp_get(ks->nk_map, &(state->nk));
        if (!node_state_p || !(*node_state_p)) {
            return FOS_STATE_MISMATCH;
        }

        kernel_fs_node_state_t *node_state = *node_state_p;

        tmp_err = bwq_notify_all(node_state->bwq);
        if (tmp_err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR; // We'll say this is a fatal error for now.
        }

        thread_t *woken_thr;
        while ((tmp_err = bwq_pop(node_state->bwq, (void **)&woken_thr)) == FOS_SUCCESS) {
            woken_thr->wq = NULL;
            woken_thr->state = THREAD_STATE_DETATCHED;

            // Ok, read out the wait context please.

            file_handle_state_t * const woken_handle_state = (file_handle_state_t *)(woken_thr->wait_ctx[0]);
            void * const u_dst = (void *)(woken_thr->wait_ctx[1]);
            size_t bytes_to_read = (size_t)(woken_thr->wait_ctx[2]);
            size_t * const u_readden =  (size_t *)(woken_thr->wait_ctx[3]);

            mem_set(woken_thr->wait_ctx, 0, sizeof(woken_thr->wait_ctx));

            if (woken_handle_state->pos != old_len) {
                return FOS_STATE_MISMATCH; // Sanity check.
            }

            // We know for a fact that `bytes_appended < KS_FS_MAX_TX_LEN`
            // We also know that the position of all waiting handles = the old length of the file!
            if (bytes_to_read > bytes_appended) {
                bytes_to_read = bytes_appended;
            }

            err = fs_read(ks->fs, woken_handle_state->nk, old_len, bytes_to_read, tx_buf);

            if (err == FOS_SUCCESS) {
                err = mem_cpy_to_user(woken_thr->proc->pd, u_dst, tx_buf, bytes_to_read, NULL);
            }

            if (err == FOS_SUCCESS) {
                err = mem_cpy_to_user(woken_thr->proc->pd, u_readden, &bytes_to_read, sizeof(size_t), NULL);
            }

            if (err == FOS_SUCCESS) {
                // += here could be kinda dangerous if the user uses file handles incorrectly.
                woken_handle_state->pos = old_len + bytes_to_read;
            }

            woken_thr->ctx.eax = err;

            ks_schedule_thread(ks, woken_thr);
        }
    }

    // Otherwise, we finish up our write!

    DUAL_RET_FOS_ERR(err, calling_thread); // NOTE an error here would be pretty bad tbh.

    state->pos += bytes_to_write;

    err = mem_cpy_to_user(calling_thread->proc->pd, u_written, &bytes_to_write, sizeof(size_t), NULL);
    DUAL_RET(calling_thread, err, FOS_SUCCESS);
}

fernos_error_t ks_fs_read(kernel_state_t *ks, file_handle_t fh, void *u_dst, size_t len, size_t *u_readden) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    DUAL_RET_COND(!u_dst || !u_readden, ks->curr_thread, FOS_BAD_ARGS, FOS_SUCCESS);

    fernos_error_t err;
    process_t *proc = ks->curr_thread->proc;

    file_handle_state_t *state = idtb_get(proc->file_handle_table, fh);
    DUAL_RET_COND(!state, ks->curr_thread, FOS_INVALID_INDEX, FOS_SUCCESS);

    fs_node_info_t info;
    err = fs_get_node_info(ks->fs, state->nk, &info);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    if (info.is_dir) { // Quick sanity check!
        return FOS_STATE_MISMATCH;
    }

    uint8_t rx_buf[KS_FS_TX_MAX_LEN];

    if (state->pos > info.len) {
        return FOS_STATE_MISMATCH; // This should never ever happen!
    }

    if (state->pos < info.len) { // We can return right now!
        const size_t bytes_left = info.len - state->pos;
        size_t bytes_to_read = len;

        if (bytes_to_read > bytes_left) {
            bytes_to_read = bytes_left;
        }

        if (bytes_to_read > KS_FS_TX_MAX_LEN) {
            bytes_to_read = KS_FS_TX_MAX_LEN;
        }

        err = fs_read(ks->fs, state->nk, state->pos, bytes_to_read, rx_buf);
        DUAL_RET_FOS_ERR(err, ks->curr_thread);

        err = mem_cpy_to_user(proc->pd, u_dst, rx_buf, bytes_to_read, NULL);
        DUAL_RET_FOS_ERR(err, ks->curr_thread);

        err = mem_cpy_to_user(proc->pd, u_readden, &bytes_to_read, sizeof(size_t), NULL);
        DUAL_RET_FOS_ERR(err, ks->curr_thread);
        
        // SUCCESS!

        state->pos += bytes_to_read;

        DUAL_RET(ks->curr_thread, FOS_SUCCESS, FOS_SUCCESS);
    }

    // pos == len (usually means blocking)

    if (state->pos == SIZE_MAX) { // In this case, the file will not grow any larger ever!
        const size_t zero = 0;
        err = mem_cpy_to_user(proc->pd, u_readden, &zero, sizeof(size_t), NULL);
        DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
    }

    // Blocking case!

    kernel_fs_node_state_t **node_state_p = mp_get(ks->nk_map, &(state->nk));
    if (!node_state_p || !(*node_state_p)) {
        return FOS_STATE_MISMATCH;
    }

    kernel_fs_node_state_t *node_state = *node_state_p;

    thread_t *waiting_thr = ks->curr_thread;

    err = bwq_enqueue(node_state->bwq, waiting_thr);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    ks_deschedule_thread(ks, waiting_thr);

    waiting_thr->wq = (wait_queue_t *)(node_state->bwq);
    waiting_thr->wait_ctx[0] = (uint32_t)state;
    waiting_thr->wait_ctx[1] = (uint32_t)u_dst;
    waiting_thr->wait_ctx[2] = len;
    waiting_thr->wait_ctx[3] = (uint32_t)u_readden;
    waiting_thr->state = THREAD_STATE_WAITING;
    
    return FOS_SUCCESS;
}

fernos_error_t ks_fs_flush(kernel_state_t *ks, file_handle_t fh) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }
    
    fernos_error_t err;

    process_t *proc = ks->curr_thread->proc;

    if (fh == FOS_MAX_FILE_HANDLES_PER_PROC) {
        err = fs_flush(ks->fs, NULL);
        DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
    }

    file_handle_state_t *state = idtb_get(proc->file_handle_table, fh);

    if (!state) {
        DUAL_RET(ks->curr_thread, FOS_INVALID_INDEX, FOS_SUCCESS);
    }

    err = fs_flush(ks->fs, state->nk);
    DUAL_RET(ks->curr_thread, err, FOS_SUCCESS);
}

