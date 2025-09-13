
#include "k_startup/plugin.h"
#include "k_startup/plugin_fs.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"
#include "k_bios_term/term.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"

static fernos_error_t delete_plugin_fs(plugin_t *plg);
static fernos_error_t plg_fs_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_fs_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_fs_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_FS_IMPL = {
    .delete_plugin = delete_plugin_fs,

    .plg_cmd = plg_fs_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = plg_fs_on_fork_proc,
    .plg_on_reap_proc = plg_fs_on_reap_proc
};

static fernos_error_t copy_fs_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_fs_handle_state(handle_state_t *hs);
static fernos_error_t fs_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
static fernos_error_t fs_hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
static fernos_error_t fs_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

const handle_state_impl_t FS_HS_IMPL = {
    .copy_handle_state = copy_fs_handle_state,
    .delete_handle_state = delete_fs_handle_state,
    .hs_write = fs_hs_write,
    .hs_read = fs_hs_read,
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

static fernos_error_t delete_plugin_fs(plugin_t *plg) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    // It's kinda difficult to clean up this plugin as it will have open handles
    // littered throughout userspace.
    //
    // So this destructory doesn't really delete anything, it just flushes the file system
    // and tells the OS to shutdown.

    fs_flush(plg_fs->fs, NULL);

    return FOS_ABORT_SYSTEM; 
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
        DUAL_RET(thr, FOS_INVALID_INDEX, FOS_SUCCESS);
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
            err = info.is_dir ? FOS_STATE_MISMATCH : err;
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
        }

        if (hs) {
            init_base_handle((handle_state_t *)hs, &FS_HS_IMPL, plg_fs->super.ks, proc, h);
            *(plugin_fs_t **)&(hs->plg_fs) = plg_fs;
            hs->pos = 0;
            *(fs_node_key_t *)&(hs->nk) = kernel_nk;
        } else {
            err = FOS_NO_MEM;
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

    plugin_fs_nk_map_entry_t *nk_entry = mp_get(plg_fs->nk_map, &parent_cwd);
    if (!nk_entry) {
        return FOS_STATE_MISMATCH;
    }
    nk_entry->references++;

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

static fernos_error_t copy_fs_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_fs_handle_state(handle_state_t *hs);
static fernos_error_t fs_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
static fernos_error_t fs_hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
static fernos_error_t fs_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

