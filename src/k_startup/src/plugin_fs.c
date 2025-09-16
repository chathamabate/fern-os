
#include "k_startup/plugin.h"
#include "k_startup/plugin_fs.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"

static void plg_fs_on_shutdown(plugin_t *plg);
static fernos_error_t plg_fs_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_fs_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_fs_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_FS_IMPL = {
    .plg_on_shutdown = plg_fs_on_shutdown,
    .plg_cmd = plg_fs_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = plg_fs_on_fork_proc,
    .plg_on_reap_proc = plg_fs_on_reap_proc
};

static fernos_error_t copy_fs_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_fs_handle_state(handle_state_t *hs);
static fernos_error_t fs_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
static fernos_error_t fs_hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
static fernos_error_t fs_hs_wait(handle_state_t *hs);
static fernos_error_t fs_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

const handle_state_impl_t FS_HS_IMPL = {
    .copy_handle_state = copy_fs_handle_state,
    .delete_handle_state = delete_fs_handle_state,
    .hs_write = fs_hs_write,
    .hs_read = fs_hs_read,
    .hs_wait = fs_hs_wait,
    .hs_cmd = fs_hs_cmd
};

plugin_t *new_plugin_fs(kernel_state_t *ks, file_sys_t *fs) {
    if (!ks || !fs) {
        return NULL;
    }

    plugin_fs_t *plg_fs = al_malloc(ks->al, sizeof(plugin_fs_t));
    map_t *nk_map = new_chained_hash_map(ks->al, sizeof(fs_node_key_t), 
            sizeof(plugin_fs_nk_map_entry_t *), 3, 
            fs_get_key_equator(fs), fs_get_key_hasher(fs));
    
    fs_node_key_t root_nk;
    fernos_error_t new_key_err = fs_new_key(fs, NULL, "/", &root_nk);

    plugin_fs_nk_map_entry_t *nk_entry = al_malloc(ks->al, sizeof(plugin_fs_nk_map_entry_t));
    basic_wait_queue_t *bwq = new_basic_wait_queue(ks->al);

    fernos_error_t put_err = FOS_UNKNWON_ERROR;
    if (nk_map && new_key_err == FOS_SUCCESS) {
        put_err = mp_put(nk_map, (void *)&root_nk, (const void *)&nk_entry);
    }

    if (!plg_fs || !nk_map || new_key_err != FOS_SUCCESS || !nk_entry || !bwq || put_err != FOS_SUCCESS) {
        delete_wait_queue((wait_queue_t *)bwq);
        al_free(ks->al, nk_entry);
        fs_delete_key(fs, root_nk);
        delete_map(nk_map);
        al_free(ks->al, plg_fs);

        return NULL;
    }

    // Ok should be smooth sailing from here.

    size_t root_nk_references = 0;
    for (size_t i = 0; i < FOS_MAX_PROCS; i++) {
        if (idtb_get(ks->proc_table, i)) {
            root_nk_references++;
            plg_fs->cwds[i] = root_nk;
        } else {
            plg_fs->cwds[i] = NULL;
        }
    }

    if (root_nk_references > 0) {
        nk_entry->bwq = bwq;
        nk_entry->references = root_nk_references;
    } else {
        // This is the case where there were no processes at all. (Which should never happen)
        // We'll still succeed though.

        mp_remove(nk_map, &root_nk);

        delete_wait_queue((wait_queue_t *)bwq);
        al_free(ks->al, nk_entry);
        fs_delete_key(fs, root_nk);
    }

    init_base_plugin((plugin_t *)plg_fs, &PLUGIN_FS_IMPL, ks);
    *(file_sys_t **)&(plg_fs->fs) = fs;
    *(map_t **)&(plg_fs->nk_map) = nk_map;

    return (plugin_t *)plg_fs;
}

static void plg_fs_on_shutdown(plugin_t *plg) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    // Flush the full file system, do nothign else.
    fs_flush(plg_fs->fs, NULL);
}

static fernos_error_t plg_fs_register_nk(plugin_fs_t *plg_fs, fs_node_key_t nk, fs_node_key_t *kernel_nk) {
    fernos_error_t err;

    if (!nk) {
        return FOS_ABORT_SYSTEM; // If we use this incorrectly within the kernel, we wrote something
                                 // very wrong.
    }

    const fs_node_key_t *k_nk_p;
    plugin_fs_nk_map_entry_t **nk_entry_p;

    // First see if the given node key already exists in the node key map.
    err = mp_get_kvp(plg_fs->nk_map, &nk, (const void **)&k_nk_p, (void **)&nk_entry_p);
    if (err == FOS_SUCCESS) {
        fs_node_key_t k_nk = *k_nk_p; 
        plugin_fs_nk_map_entry_t *nk_entry = *nk_entry_p;
        if (!k_nk || !nk_entry) {
            return FOS_ABORT_SYSTEM;
        }

        nk_entry->references++;

        if (kernel_nk) {
            *kernel_nk = k_nk;
        }

        return FOS_SUCCESS;
    }

    // Otherwise, we must create an entry in the map for a copy of nk.

    fs_node_key_t nk_copy = fs_new_key_copy(plg_fs->fs, nk);
    plugin_fs_nk_map_entry_t *entry = al_malloc(plg_fs->super.ks->al, sizeof(plugin_fs_nk_map_entry_t));
    basic_wait_queue_t *bwq = new_basic_wait_queue(plg_fs->super.ks->al);

    if (nk_copy && entry && bwq) {
        entry->references = 1;
        entry->bwq = bwq;
        err = mp_put(plg_fs->nk_map, &nk_copy, &entry);
    }

    if (!nk_copy || !entry || !bwq || err != FOS_SUCCESS) {
        mp_remove(plg_fs->nk_map, &nk_copy);
        delete_wait_queue((wait_queue_t *)bwq);
        al_free(plg_fs->super.ks->al, entry);
        fs_delete_key(plg_fs->fs, nk_copy);

        return FOS_UNKNWON_ERROR;
    }

    *kernel_nk = nk_copy;

    return FOS_SUCCESS;
}

static fernos_error_t plg_fs_deregister_nk(plugin_fs_t *plg_fs, fs_node_key_t nk) {
    fernos_error_t err;

    if (!nk) {
        return FOS_ABORT_SYSTEM;
    }

    const fs_node_key_t *kernel_nk_p;
    plugin_fs_nk_map_entry_t **nk_entry_p;

    err = mp_get_kvp(plg_fs->nk_map, &nk, (const void **)&kernel_nk_p, (void **)&nk_entry_p);
    if (err != FOS_SUCCESS) {
        return FOS_ABORT_SYSTEM;
    }

    fs_node_key_t kernel_nk = *kernel_nk_p; 
    plugin_fs_nk_map_entry_t *nk_entry = *nk_entry_p;
    if (!kernel_nk || !nk_entry) {
        return FOS_ABORT_SYSTEM;
    }

    if (--(nk_entry->references) == 0) {
        err = bwq_notify_all(nk_entry->bwq);
        if (err != FOS_SUCCESS) {
            return FOS_ABORT_SYSTEM;
        }

        thread_t *woken_thread;
        while ((err = bwq_pop(nk_entry->bwq, (void **)&woken_thread)) == FOS_SUCCESS) {
            woken_thread->state = THREAD_STATE_DETATCHED; 
            mem_set(&(woken_thread->wait_ctx), 0, sizeof(woken_thread->wait_ctx));
            woken_thread->ctx.eax = FOS_STATE_MISMATCH; 

            ks_schedule_thread(plg_fs->super.ks, woken_thread);
        }

        if (err != FOS_EMPTY) {
            return FOS_ABORT_SYSTEM;
        }

        delete_wait_queue((wait_queue_t *)nk_entry->bwq);
        al_free(plg_fs->super.ks->al, nk_entry);
        mp_remove(plg_fs->nk_map, &kernel_nk);
        fs_delete_key(plg_fs->fs, kernel_nk);
    }

    return FOS_SUCCESS;
}

static fernos_error_t plg_fs_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    thread_t *thr = plg->ks->curr_thread;
    process_t *proc = thr->proc;
    proc_id_t pid = proc->pid;

    // Is this command even valid though?
    if (cmd >= PLG_FILE_SYS_NUM_CMDS) {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    // Flush all is kinda special because it does not take a string as the first argument.
    // So, we'll just deal with it first than move on.
    if (cmd == PLG_FS_PCID_FLUSH) {
        err = fs_flush(plg_fs->fs, NULL);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    const char *u_path = (const char *)arg0;
    size_t path_len = arg1;

    if (!u_path || path_len > FS_MAX_PATH_LEN || path_len == 0) {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    char path[FS_MAX_PATH_LEN + 1];
    err = mem_cpy_from_user(path, thr->proc->pd, u_path, path_len, NULL);
    if (err != FOS_SUCCESS) {
        DUAL_RET(thr, err, FOS_SUCCESS); // some sort of copy error?
    }
    path[path_len] = '\0';

    fs_node_key_t cwd = plg_fs->cwds[pid];
    if (!cwd) {
        return FOS_STATE_MISMATCH; // should never happen.
    }

    // Just gonna do this all in place cause why not.

    switch (cmd) {

    /*
     * Set new working directory.
     *
     * returns FOS_INVALID_INDEX if the given path does not exist.
     * returns FOS_STATE_MISMATCH if path doesn't point to a directory.
     */
    case PLG_FS_PCID_SET_WD: {
        fs_node_key_t nk;
        err =  fs_new_key(plg_fs->fs, cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, thr);

        fs_node_info_t info;
        err = fs_get_node_info(plg_fs->fs, nk, &info);
        if (err != FOS_SUCCESS || !(info.is_dir)) {
            fs_delete_key(plg_fs->fs, nk);
            err = !(info.is_dir) ? FOS_STATE_MISMATCH : err;
            DUAL_RET(thr, err, FOS_SUCCESS);
        }

        // For now, if there is any error, dergistering/registering, we'll just abort.
        
        // Deregister old CWD
        err = plg_fs_deregister_nk(plg_fs, plg_fs->cwds[pid]);
        if (err != FOS_SUCCESS) {
            return err;
        }

        plg_fs->cwds[pid] = NULL;

        fs_node_key_t kernel_nk;
        err = plg_fs_register_nk(plg_fs, nk, &kernel_nk);
        if (err != FOS_SUCCESS) {
            return err;
        }

        fs_delete_key(plg_fs->fs, nk);
        plg_fs->cwds[pid] = kernel_nk;

        DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
    }

    /*
     * Create a new file.
     *
     * returns FOS_INVALID_INDEX if the directory component of the path does not exist.
     * returns FOS_IN_USE if the given path already exists.
     */
    case PLG_FS_PCID_TOUCH: {
        err = fs_touch_path(plg_fs->fs, cwd, path, NULL);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    /*
     * Create a new directory.
     *
     * returns FOS_INVALID_INDEX if the directory component of the path does not exist.
     * returns FOS_IN_USE if the given path already exists.
     */
    case PLG_FS_PCID_MKDIR: {
        err = fs_mkdir_path(plg_fs->fs, cwd, path, NULL);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    /*
     * Remove a file or directory.
     *
     * If this node trying to be deleted is currently refercence by any process, FOS_IN_USE
     * is returned.
     *
     * returns FOS_INVALID_INDEX if the given path does not exist.
     * FOS_IN_USE is also returned when trying to delete a non-empty directory.
     */
    case PLG_FS_PCID_REMOVE: {
        fs_node_key_t nk;

        err =  fs_new_key(plg_fs->fs, cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, thr);

        if (mp_get(plg_fs->nk_map, &nk)) {
            // This key is referenced by another process!
            fs_delete_key(plg_fs->fs, nk);
            DUAL_RET(thr, FOS_IN_USE, FOS_SUCCESS);
        }

        fs_node_info_t info;
        err = fs_get_node_info(plg_fs->fs, nk, &info);

        // We should be able to delete the node key here regardless.
        fs_delete_key(plg_fs->fs, nk);

        if (err != FOS_SUCCESS) {
            DUAL_RET(thr, err, FOS_SUCCESS);
        }

        if (info.is_dir && info.len > 0) {
            // Non empty directory!
            DUAL_RET(thr, FOS_IN_USE, FOS_SUCCESS);
        }

        // We made it here, the file being reference is not referenced by any processes!
        // And, if it is a directory, it is empty!

        err = fs_remove_path(plg_fs->fs, cwd, path);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    /*
     * Attempt to get information about a file or directory.
     *
     * returns FOS_INVALID_INDEX if the given path does not exist.
     * On success, the information is written to `*info`.
     */
    case PLG_FS_PCID_GET_INFO: {
        fs_node_info_t *u_info = (fs_node_info_t *)arg2; 
        if (!u_info) {
            DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
        }

        fs_node_key_t nk;

        err =  fs_new_key(plg_fs->fs, cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, thr);

        fs_node_info_t info;
        err = fs_get_node_info(plg_fs->fs, nk, &info);
        if (err == FOS_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_info, &info, sizeof(fs_node_info_t), NULL);
        }

        fs_delete_key(plg_fs->fs, nk);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    /*
     * Get the name of a node within a directory at a given index.
     *
     * `child_name` should be a buffer in with size at least `FS_MAX_FILENAME_LEN + 1`.
     *
     * returns FOS_INVALID_INDEX if the given path does not exist.
     * returns FOS_STATE_MISMATCH if given path leads to a file.
     * On success, FOS_SUCCESS is returned in the calling thread, and the child's name is written
     * to `*child_name` in userspace.
     *
     * If `index` overshoots the end of the directory, FOS_SUCCESS is still returned, but
     * `\0` is written to `child_name`.
     */
    case PLG_FS_PCID_GET_CHILD_NAME: {
        size_t index = arg2;
        char *u_child_name = (char *)arg3;

        if (!u_child_name) {
            DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
        }

        char child_names[1][FS_MAX_FILENAME_LEN + 1];

        fs_node_key_t nk;

        err =  fs_new_key(plg_fs->fs, cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, thr);

        err = fs_get_child_names(plg_fs->fs, nk, index, 1, child_names);
        fs_delete_key(plg_fs->fs, nk);

        DUAL_RET_FOS_ERR(err, thr);

        err = mem_cpy_to_user(proc->pd, u_child_name, child_names[0], str_len(child_names[0]) + 1, NULL);

        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    /*
     * Open an existing file.
     * 
     * When a file is opened, it's corresponding handle will start at position 0. (i.e. the 
     * beginning of the file)
     *
     * returns FOS_INVALID_INDEX if the given path does not exist.
     * returns FOS_STATE_MISMATCH if the given path is a directory.
     * FOS_EMPTY is returned when we are out of space in the file handle table for this
     * process!
     */
    case PLG_FS_PCID_OPEN: {
        handle_t *u_h = (handle_t *)arg2;
        if (!u_h) {
            DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
        }

        fs_node_key_t nk;

        err = fs_new_key(plg_fs->fs, cwd, path, &nk);
        DUAL_RET_FOS_ERR(err, thr);

        fs_node_info_t info;
        err = fs_get_node_info(plg_fs->fs, nk, &info);
        if (err != FOS_SUCCESS || info.is_dir) {
            fs_delete_key(plg_fs->fs, nk);
            err = info.is_dir ? FOS_STATE_MISMATCH : err;
            DUAL_RET(thr, err, FOS_SUCCESS);
        }

        // Ok, our node key exists and points to a file (not a directory)
        // We should be able to succeed from here given there's no unexpected error.

        fs_node_key_t kernel_nk = NULL;
        err = plg_fs_register_nk(plg_fs, nk, &kernel_nk);
        fs_delete_key(plg_fs->fs, nk); // Do this regardless.
        
        if (err != FOS_SUCCESS) {
            return err; // Treat a register failure as catastrophic.
        }

        const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);
        handle_t h = idtb_pop_id(proc->handle_table);
        if (h == NULL_HANDLE) {
            err = FOS_EMPTY;
        }

        plugin_fs_handle_state_t *hs = NULL;
        if (err == FOS_SUCCESS) {
            hs = al_malloc(plg_fs->super.ks->al, sizeof(plugin_fs_handle_state_t));
            if (!hs) {
                err = FOS_NO_MEM;
            }
        }

        if (err == FOS_SUCCESS) {
            init_base_handle((handle_state_t *)hs, &FS_HS_IMPL, plg_fs->super.ks, proc, h);
            *(plugin_fs_t **)&(hs->plg_fs) = plg_fs;
            hs->pos = 0;
            *(fs_node_key_t *)&(hs->nk) = kernel_nk;
        } 

        if (err == FOS_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_h, &h, sizeof(handle_t), NULL);
        }

        if (err != FOS_SUCCESS) {
            al_free(plg_fs->super.ks->al, hs);
            idtb_push_id(proc->handle_table, h);
            if (kernel_nk && plg_fs_deregister_nk(plg_fs, kernel_nk) != FOS_SUCCESS) {
                return FOS_ABORT_SYSTEM;
            }

            DUAL_RET(thr, err, FOS_SUCCESS);
        }

        idtb_set(proc->handle_table, h, hs);

        DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
    }

    default: { // This will never run.
        return FOS_STATE_MISMATCH;
    }
    }
}

static fernos_error_t plg_fs_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    process_t *child = idtb_get(plg_fs->super.ks->proc_table, cpid);
    if (!child) {
        return FOS_STATE_MISMATCH; // Something is very wrong if this is the case.
    }

    process_t *parent = child->parent;
    fs_node_key_t parent_cwd = plg_fs->cwds[parent->pid];
    if (!parent_cwd) {
        return FOS_STATE_MISMATCH;
    }

    // Copy over parent CWD
    plg_fs->cwds[child->pid] = parent_cwd;

    plugin_fs_nk_map_entry_t **nk_entry = mp_get(plg_fs->nk_map, &parent_cwd);
    if (!nk_entry || !*nk_entry) {
        return FOS_STATE_MISMATCH;
    }
    (*nk_entry)->references++;

    // The copy function inside each handle should handle increasing the reference count for
    // each copied handle. We only need to deal with copying over the cwd here.

    return FOS_SUCCESS;    
}

static fernos_error_t plg_fs_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    fs_node_key_t cwd = plg_fs->cwds[rpid];
    if (!cwd) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err = plg_fs_deregister_nk(plg_fs, cwd);
    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

static fernos_error_t copy_fs_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    // Ok, let's make our new handle.

    plugin_fs_handle_state_t *fs_hs_copy = al_malloc(hs->ks->al, sizeof(plugin_fs_handle_state_t));
    if (!fs_hs_copy) {
        return FOS_NO_MEM;
    }

    // Because we are copying a handle state, we expect its node key to already be in
    // the map, we don't need to call register, we can just incrememnt the count.

    plugin_fs_nk_map_entry_t **nk_entry = mp_get(plg_fs->nk_map, &(fs_hs->nk));
    if (!nk_entry || !*nk_entry) {
        return FOS_ABORT_SYSTEM; // Catastrophic error.
    }
    (*nk_entry)->references++;

    init_base_handle((handle_state_t *)fs_hs_copy, hs->impl, hs->ks, proc, hs->handle);
    *(plugin_fs_t **)&(fs_hs_copy->plg_fs) = plg_fs;
    fs_hs_copy->pos = fs_hs->pos;
    *(fs_node_key_t *)&(fs_hs_copy->nk) = fs_hs->nk;

    // Remember, we don't need to modify the table here! That's done outside this function for us!

    *out = (handle_state_t *)fs_hs_copy;
    return FOS_SUCCESS;
}

static fernos_error_t delete_fs_handle_state(handle_state_t *hs) {
    fernos_error_t err;

    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    // We'll flush on close and not really worry about errors.
    fs_flush(plg_fs->fs, fs_hs->nk);

    err = plg_fs_deregister_nk(plg_fs, fs_hs->nk);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // We're in the clear!

    al_free(fs_hs->super.ks->al, fs_hs);

    return FOS_SUCCESS;
}

/**
 * Write the contents of `src` to the referenced file.
 *
 * If `len` overshoots the end of the file, the file will be expanded as necessary.
 * If the given file handle's position is already SIZE_MAX, FOS_NO_SPACE will be returned
 * in the calling thread.
 *
 * The total number of bytes written is written to `*written`.
 * FOS_SUCCESS does NOT mean all bytes were written!!! (This will likely write in chunks to 
 * prevent being in the kernel for too long)
 */
static fernos_error_t fs_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    fernos_error_t err;

    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    thread_t *thr = hs->ks->curr_thread;

    if (!u_src || !u_written) {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    if (fs_hs->pos == SIZE_MAX)  {
        DUAL_RET(thr, FOS_NO_SPACE, FOS_SUCCESS);
    }

    fs_node_info_t info;
    err = fs_get_node_info(plg_fs->fs, fs_hs->nk, &info);
    DUAL_RET_FOS_ERR(err, thr);

    if (info.is_dir) {
        return FOS_STATE_MISMATCH; // Sanity check.
    }

    const size_t old_len = info.len;

    if (fs_hs->pos > old_len) {
        // Another quick sanity check. (This should never happen!)
        return FOS_STATE_MISMATCH;
    }

    // Ok, expansion may be necessary!

    size_t bytes_to_write = len;
    if (fs_hs->pos + bytes_to_write < fs_hs->pos) { // check wrap.
        bytes_to_write = SIZE_MAX - fs_hs->pos;
    }

    if (bytes_to_write > KS_FS_TX_MAX_LEN) {
        bytes_to_write = KS_FS_TX_MAX_LEN;
    }

    uint8_t tx_buf[KS_FS_TX_MAX_LEN];
    err = mem_cpy_from_user(tx_buf, thr->proc->pd, u_src, bytes_to_write, NULL);
    DUAL_RET_FOS_ERR(err, thr);

    const size_t bytes_left = old_len - fs_hs->pos;

    // First off, do we need to expand our file?
    if (bytes_left < bytes_to_write) { // expansion is needed.
        err = fs_resize(plg_fs->fs, fs_hs->nk, fs_hs->pos + bytes_to_write);
        DUAL_RET_FOS_ERR(err, thr);
    }

    // Save this error for later.
    err = fs_write(plg_fs->fs, fs_hs->nk, fs_hs->pos, bytes_to_write, tx_buf);

    if (err == FOS_SUCCESS) {
        fs_hs->pos += bytes_to_write;
        err = mem_cpy_to_user(thr->proc->pd, u_written, &bytes_to_write, sizeof(size_t), NULL);
    }

    thr->ctx.eax = err;

    // Ok, so, if we had to expand our file, that means we must wake up all our blocked threads!
    // We do this regardless of whether or not the above write succeeded or not.
    // This could potentially result in faulty data read in the case where the write fails.
    // But whatever.
    if (bytes_left < bytes_to_write) {
        plugin_fs_nk_map_entry_t **nk_entry_p = mp_get(plg_fs->nk_map, &(fs_hs->nk));
        if (!nk_entry_p || !(*nk_entry_p)) {
            return FOS_STATE_MISMATCH;
        }

        basic_wait_queue_t *bwq = (*nk_entry_p)->bwq;
        err = bwq_notify_all(bwq);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR; // We'll say this is a fatal error for now.
        }

        thread_t *woken_thr;
        while ((err = bwq_pop(bwq, (void **)&woken_thr)) == FOS_SUCCESS) {
            woken_thr->wq = NULL;
            woken_thr->state = THREAD_STATE_DETATCHED;

            // The wait context will just be the handle the blocked with `hs_wait`.
            // Confirms its position makes sense.

            plugin_fs_handle_state_t * const woken_handle_state = (plugin_fs_handle_state_t *)(woken_thr->wait_ctx[0]);
            mem_set(woken_thr->wait_ctx, 0, sizeof(woken_thr->wait_ctx));

            if (woken_handle_state->pos != old_len) {
                return FOS_STATE_MISMATCH; // Sanity check.
            }

            woken_thr->ctx.eax = FOS_SUCCESS;

            ks_schedule_thread(plg_fs->super.ks, woken_thr);
        }
    }

    return FOS_SUCCESS;
}

/**
 * Non-Blocking read.
 *
 * Read from a file into userspace buffer `dst`.
 *
 * If the position of `fh` = len(file), this function returns FOS_EMTPY to the user thread.
 *
 * Just like with `sc_fs_write`, FOS_SUCCESS does NOT mean `len` bytes were read. 
 * On Success, always check what is written to `readden` to confirm the actual number of 
 * read bytes.
 */
static fernos_error_t fs_hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden) {
    fernos_error_t err;

    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    thread_t *thr = hs->ks->curr_thread;

    if (!u_dst || !u_readden) {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    fs_node_info_t info;
    err = fs_get_node_info(plg_fs->fs, fs_hs->nk, &info);
    DUAL_RET_COND(err != FOS_SUCCESS, thr, FOS_UNKNWON_ERROR, FOS_SUCCESS);

    // A file handle should never point to a directory!
    if (info.is_dir) {
        return FOS_STATE_MISMATCH;
    }

    if (fs_hs->pos > info.len) {
        return FOS_STATE_MISMATCH;
    }

    if (fs_hs->pos == info.len) { // Nothing to read case.
        DUAL_RET(thr, FOS_EMPTY, FOS_SUCCESS);
    }

    // pos < len. Non-blocking read case.

    const size_t bytes_left = info.len - fs_hs->pos;
    size_t bytes_to_read = len;

    if (bytes_to_read > bytes_left) {
        bytes_to_read = bytes_left;
    }

    if (bytes_to_read > KS_FS_TX_MAX_LEN) {
        bytes_to_read = KS_FS_TX_MAX_LEN;
    }

    uint8_t rx_buf[KS_FS_TX_MAX_LEN];

    err = fs_read(plg_fs->fs, fs_hs->nk, fs_hs->pos, bytes_to_read, rx_buf);
    DUAL_RET_COND(err != FOS_SUCCESS, thr, FOS_UNKNWON_ERROR, FOS_SUCCESS);

    err = mem_cpy_to_user(hs->proc->pd, u_dst, rx_buf, bytes_to_read, NULL);
    DUAL_RET_COND(err != FOS_SUCCESS, thr, FOS_UNKNWON_ERROR, FOS_SUCCESS);

    err = mem_cpy_to_user(hs->proc->pd, u_readden, &bytes_to_read, sizeof(size_t), NULL);
    DUAL_RET_COND(err != FOS_SUCCESS, thr, FOS_UNKNWON_ERROR, FOS_SUCCESS);
    
    // SUCCESS!

    fs_hs->pos += bytes_to_read;

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

static fernos_error_t fs_hs_wait(handle_state_t *hs) {
    fernos_error_t err;

    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    thread_t *thr = hs->ks->curr_thread;

    fs_node_info_t info;
    err = fs_get_node_info(plg_fs->fs, fs_hs->nk, &info);
    DUAL_RET_FOS_ERR(err, thr);

    if (info.is_dir) {
        return FOS_STATE_MISMATCH;
    }

    if (fs_hs->pos > info.len) {
        return FOS_STATE_MISMATCH;
    }

    if (fs_hs->pos == info.len) { // Potentially a blocking case.
        if (info.len == SIZE_MAX) {
            DUAL_RET(thr, FOS_EMPTY, FOS_SUCCESS);
        }

        // Block!
        plugin_fs_nk_map_entry_t **nk_entry_p = mp_get(plg_fs->nk_map, &(fs_hs->nk));
        if (!nk_entry_p || !*nk_entry_p) {
            return FOS_STATE_MISMATCH;
        }

        basic_wait_queue_t *bwq = (*nk_entry_p)->bwq;

        err = bwq_enqueue(bwq, thr);
        DUAL_RET_COND(err != FOS_SUCCESS, thr, FOS_UNKNWON_ERROR, FOS_SUCCESS);

        ks_deschedule_thread(hs->ks, thr);

        thr->wq = (wait_queue_t *)bwq;
        thr->wait_ctx[0] = (uint32_t)fs_hs;
        thr->state = THREAD_STATE_WAITING;
    }

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

static fernos_error_t fs_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_fs_handle_state_t *fs_hs = (plugin_fs_handle_state_t *)hs;
    plugin_fs_t *plg_fs = fs_hs->plg_fs;

    thread_t *thr = hs->ks->curr_thread;

    switch (cmd) {

    /*
     * Move a handle's position.
     *
     * If `pos` > len(file), then the `fh`'s position will be set to len(file).
     *
     * Using SIZE_MAX as the position will thus always bring the handle position to the end of the
     * file. (Remember, we will enforce the max file size of SIZE_MAX, and thus a maximum addressable
     * byte of SIZE_MAX - 1)
     */
    case PLG_FS_HCID_SEEK: {
        size_t new_pos = (size_t)arg0;

        fs_node_info_t info;
        err = fs_get_node_info(plg_fs->fs, fs_hs->nk, &info);
        DUAL_RET_FOS_ERR(err, thr);

        if (info.is_dir) {
            return FOS_STATE_MISMATCH; // Sanity check.
        }

        if (new_pos > info.len) {
            new_pos = info.len;
        }

        fs_hs->pos = new_pos;
        DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
    }

    case PLG_FS_HCID_FLUSH: {
        err = fs_flush(plg_fs->fs, fs_hs->nk);
        DUAL_RET(thr, err, FOS_SUCCESS);
    }

    default: {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    }
}


