
#include "k_startup/plugin.h"
#include "k_startup/plugin_fs.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"
#include "k_bios_term/term.h"

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

static fernos_error_t plg_fs_deregister_nk(plugin_fs_t *plg_fs, fs_node_key_t nk) {
    fernos_error_t err;

    if (!nk) {
        return FOS_BAD_ARGS;
    }

    const fs_node_key_t *kernel_nk_p;
    plugin_fs_nk_map_entry_t **nk_entry_p;

    err = mp_get_kvp(plg_fs->nk_map, &nk, (const void **)&kernel_nk_p, (void **)&nk_entry_p);
    if (err != FOS_SUCCESS) {
        return FOS_STATE_MISMATCH;
    }

    fs_node_key_t kernel_nk = *kernel_nk_p; 
    plugin_fs_nk_map_entry_t *nk_entry = *nk_entry_p;
    if (!kernel_nk || !nk_entry) {
        return FOS_STATE_MISMATCH;
    }

    if (--(nk_entry->references) == 0) {
        err = bwq_notify_all(nk_entry->bwq);
        if (err != FOS_SUCCESS) {
            return err;
        }

        thread_t *woken_thread;
        while ((err = bwq_pop(nk_entry->bwq, (void **)&woken_thread)) == FOS_SUCCESS) {
            woken_thread->state = THREAD_STATE_DETATCHED; 
            mem_set(&(woken_thread->wait_ctx), 0, sizeof(woken_thread->wait_ctx));
            woken_thread->ctx.eax = FOS_STATE_MISMATCH; 

            ks_schedule_thread(plg_fs->super.ks, woken_thread);
        }

        if (err != FOS_EMPTY) {
            return err;
        }

        delete_wait_queue((wait_queue_t *)nk_entry->bwq);
        al_free(plg_fs->super.ks->al, nk_entry);
        mp_remove(plg_fs->nk_map, &kernel_nk);
        fs_delete_key(plg_fs->fs, kernel_nk);
    }

    return FOS_SUCCESS;
}

static fernos_error_t plg_fs_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    // Remeber, this is called by a thread.
    DUAL_RET(plg->ks->curr_thread, FOS_SUCCESS, FOS_SUCCESS);
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
