
#include "k_startup/state_fs.h"

#include "k_startup/fwd_defs.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/thread.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "s_block_device/file_sys.h"

/**
 * Increment a given node key's reference count in the kernels file map.
 * NOTE VERY IMPORTANT: `nk` must point to a node key that has just been newly allocated.
 * An semantically equal node key can already live in the kernel state (in the case this file/dir
 * is already referenced), but the key itself must be different. 
 *
 * (This function will delete the given key if it already appears in the file map)
 *
 * FOS_STATE_MISMATCH being returned means something is seriously wrong with `ks`.
 * Other error can be returned to the calling thread.
 */
static fernos_error_t ks_fs_register_nk(kernel_state_t *ks, fs_node_key_t *nk) {
    fernos_error_t err;

    if (!nk || !(*nk)) {
        return FOS_BAD_ARGS;
    }

    kernel_fs_node_state_t **nk_state_p = mp_get(ks->open_files, nk);
    kernel_fs_node_state_t *nk_state;

    if (!nk_state_p) {
        // The given key does not appear yet, so let's give it an entry!
        nk_state = al_malloc(ks->al, sizeof(kernel_fs_node_state_t));
        if (!nk_state) {
            return FOS_NO_MEM;
        }

        nk_state->references = 1;
        nk_state->twq = NULL; // No wait queue for a directory!

        // Here the given node key is consumed by being placed directly in the map.
        err = mp_put(ks->open_files, nk, &nk_state);

        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }
    } else {
        nk_state = *nk_state_p;
        if (!nk_state) {
            return FOS_STATE_MISMATCH;
        }

        nk_state->twq++;

        // Here our node key is already in the map, so we can delete the one given!
        fs_delete_key(ks->fs, *nk);
    }

    *nk = NULL;
    return FOS_SUCCESS;
}

/**
 * Decrement a node key's reference count in the kernel file map.
 * FOS_STATE_MISMATCH is returned if `nk` cannot be found in the file map
 * (signifying a fatal kernel error)
 *
 * Unlike with `ks_fs_register_nk`, this function does not consume `nk` in anyway.
 * However, if the reference count of `nk` reaches zero, it's entry in the kernel state is
 * entirely deleted. SO, if `nk` is a node key which is managed by the kernel, it may 
 * be have been deleted during this function call.
 */
static fernos_error_t ks_fs_deregister_nk(kernel_state_t *ks, fs_node_key_t nk) {
    fernos_error_t err;

    // DAMN so many fucking gotchas...
    // I think what we do is that if a reference count goes to zero, we wake up all sleeping threads?
    // That's what I think we need to do!!!

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

    err = mem_cpy_from_user(path, proc->pd, u_path, u_path_len, NULL);
    DUAL_RET_FOS_ERR(err, ks->curr_thread);

    path[u_path_len] = '\0';

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




    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len, file_handle_t *u_fh) {
    return FOS_NOT_IMPLEMENTED;
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

