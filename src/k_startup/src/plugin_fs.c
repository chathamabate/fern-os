
#include "k_startup/plugin.h"
#include "k_startup/plugin_fs.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"

static void delete_plugin_fs(plugin_t *plg);

static const plugin_impl_t PLUGIN_FS_IMPL = {
    .delete_plugin = delete_plugin_fs,

    .plg_cmd = NULL,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reap_proc = NULL
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

static void delete_plugin_fs(plugin_t *plg) {
    plugin_fs_t *plg_fs = (plugin_fs_t *)plg;

    // This needs to wake up all threads which are waiting!
    // Ok, how do I iterate over a map again?
    
    mp_reset_iter(plg_fs->nk_map);

    const fs_node_key_t *nk_p;
    plugin_fs_nk_map_entry_t **nk_entry_p;

    for (fernos_error_t err = mp_get_iter(plg_fs->nk_map, (const void **)&nk_p, (void **)&nk_entry_p);
        err != FOS_EMPTY; err = mp_next_iter(plg_fs->nk_map, (const void **)&nk_p, (void **)&nk_entry_p)) {

        fs_node_key_t nk = *nk_p;
        plugin_fs_nk_map_entry_t *nk_entry = *nk_entry_p;
         
        // Ok, schedule all threads which are woken up. On error, whatever.
        fernos_error_t tmp_err = bwq_notify_all(nk_entry->bwq);
        if (tmp_err == FOS_SUCCESS) {
            thread_t *woken_thread;
            while ((tmp_err = bwq_pop(nk_entry->bwq, (void **)&woken_thread)) == FOS_SUCCESS) {
                woken_thread->state = THREAD_STATE_DETATCHED; 
                mem_set(&(woken_thread->wait_ctx), 0, sizeof(woken_thread->wait_ctx));
                woken_thread->ctx.eax = FOS_STATE_MISMATCH; 

                ks_schedule_thread(plg_fs->super.ks, woken_thread);
            }
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
            for (plugin_fs_handle_t j = 0; j < PLG_FS_MAX_HANDLES_PER_PROC; j++) {
                plugin_fs_handle_state_t *handle_state = idtb_get(handle_table, i);
                al_free(plg_fs->super.ks->al, handle_state); // Should be OK if NULL.
            }

            delete_id_table(handle_table);
        }
    }

    // Finally, we can delete the whole plugin obj!
    al_free(plg_fs->super.ks->al, plg_fs);
}
