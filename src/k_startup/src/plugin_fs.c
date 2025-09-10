
#include "k_startup/plugin.h"
#include "k_startup/plugin_fs.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"
#include "k_bios_term/term.h"

static fernos_error_t delete_plugin_fs(plugin_t *plg);
static fernos_error_t plg_fs_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_fs_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_FS_IMPL = {
    .delete_plugin = delete_plugin_fs,

    .plg_cmd = NULL,
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


    // Create an empty handle for every process in the process table.
    bool table_allocs_failed = false;
    if (plg_fs) {
        for (proc_id_t i = 0; i < FOS_MAX_PROCS; i++) {
            if (!table_allocs_failed && idtb_get(ks->proc_table, i)) {
                id_table_t *handle_table = new_id_table(ks->al, PLG_FS_MAX_HANDLES_PER_PROC);
                if (!handle_table) {
                    table_allocs_failed = true;
                }
                plg_fs->handle_tables[i] = handle_table;
            } else {
                plg_fs->handle_tables[i] = NULL; 
            }
        }
    }

    if (!plg_fs || !nk_map || table_allocs_failed) {
        if (plg_fs) {
            for (proc_id_t i = 0; i < FOS_MAX_PROCS; i++) {
                delete_id_table(plg_fs->handle_tables[i]);
            }
        }

        delete_map(nk_map);
        al_free(ks->al, plg_fs);

        return NULL;
    }

    // SUCCESS! Write all fields!

    init_base_plugin((plugin_t *)plg_fs, &PLUGIN_FS_IMPL, ks);
    *(file_sys_t **)&(plg_fs->fs) = fs;
    *(map_t **)&(plg_fs->nk_map) = nk_map;

    return (plugin_t *)plg_fs;
}

static fernos_error_t delete_plugin_fs(plugin_t *plg) {
    fernos_error_t err;

    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    // This needs to wake up all threads which are waiting!
    // Ok, how do I iterate over a map again?
    
    mp_reset_iter(plg_fs->nk_map);

    const fs_node_key_t *nk_p;
    plugin_fs_nk_map_entry_t **nk_entry_p;

    for (err = mp_get_iter(plg_fs->nk_map, (const void **)&nk_p, (void **)&nk_entry_p);
        err == FOS_SUCCESS; err = mp_next_iter(plg_fs->nk_map, (const void **)&nk_p, (void **)&nk_entry_p)) {

        fs_node_key_t nk = *nk_p;
        plugin_fs_nk_map_entry_t *nk_entry = *nk_entry_p;
         
        // Ok, schedule all threads which are woken up. On error, whatever.
        fernos_error_t tmp_err = bwq_notify_all(nk_entry->bwq);
        if (tmp_err != FOS_SUCCESS) {
            return tmp_err;
        }

        thread_t *woken_thread;
        while ((tmp_err = bwq_pop(nk_entry->bwq, (void **)&woken_thread)) == FOS_SUCCESS) {
            woken_thread->state = THREAD_STATE_DETATCHED; 
            mem_set(&(woken_thread->wait_ctx), 0, sizeof(woken_thread->wait_ctx));
            woken_thread->ctx.eax = FOS_STATE_MISMATCH; 

            ks_schedule_thread(plg_fs->super.ks, woken_thread);
        }

        if (tmp_err != FOS_EMPTY) {
            return tmp_err;
        }

        // Finally delete the wait queue, nk_entry, and nk.
        delete_wait_queue((wait_queue_t *)(nk_entry->bwq));
        al_free(plg_fs->super.ks->al, nk_entry);
        fs_delete_key(plg_fs->fs, nk);
    }

    // Now we can delete the nk map entirely!
    
    delete_map(plg_fs->nk_map);
    
    // Now just delete all handle tables!
    
    for (proc_id_t i = 0; i < FOS_MAX_PROCS; i++) {
        id_table_t *handle_table = plg_fs->handle_tables[i];
        if (handle_table) {
            idtb_reset_iterator(handle_table);
            const plugin_fs_handle_t NULL_HANDLE = idtb_null_id(handle_table);

            for (plugin_fs_handle_t j = idtb_get_iter(handle_table); j != NULL_HANDLE; 
                    j = idtb_next(handle_table)) {
                plugin_fs_handle_state_t *handle_state = idtb_get(handle_table, j);
                al_free(plg_fs->super.ks->al, handle_state);
            }

            delete_id_table(handle_table);
        }
    }

    err = fs_flush(plg_fs->fs, NULL);
    if (err != FOS_SUCCESS) {
        return err;
    }

    delete_file_sys(plg_fs->fs);

    // Finally, we can delete the whole plugin obj!
    al_free(plg_fs->super.ks->al, plg_fs);

    return FOS_SUCCESS;
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

static fernos_error_t plg_fs_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    // Slowly getting there ugh!

    return FOS_SUCCESS;    
}

static fernos_error_t plg_fs_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    id_table_t *handle_table = plg_fs->handle_tables[rpid];
    if (!handle_table) {
        return FOS_STATE_MISMATCH;
    }

    const plugin_fs_handle_t NULL_HANDLE = idtb_null_id(handle_table);

    // Delete all handle states from the handle table!

    idtb_reset_iterator(handle_table);
    for (plugin_fs_handle_t fh = idtb_get_iter(handle_table); fh != NULL_HANDLE; 
            fh = idtb_next(handle_table)) {
        plugin_fs_handle_state_t *fh_state = idtb_get(handle_table, fh);
        plg_fs_deregister_nk(plg_fs, fh_state->nk);
        al_free(plg_fs->super.ks->al, fh_state);
    }

    // Finally delete the handle table itself!

    return FOS_SUCCESS;
}
