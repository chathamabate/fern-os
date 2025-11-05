
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/thread.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"

#include "k_startup/handle.h"
#include "s_util/misc.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"

static bool fm_key_eq(const void *k0, const void *k1) {
    return *(const futex_t **)k0 == *(const futex_t **)k1;
}

static uint32_t fm_key_hash(const void *k) {
    const futex_t *futex = *(const futex_t **)k;

    // Kinda just a random hash function here.
    return (((uint32_t)futex + 1373) * 7) + 2;
}

/**
 * Private helper for clearing the futex map. This deletes all futex wait queues
 * (WITHOUT CHECKING THEIR STATE). Then it clears the futex map.
 */
static void proc_clear_fm_map(process_t *proc) {
    // Delete all wait queues!
    basic_wait_queue_t **fwq; // Pretty confusing, but yes this should be a **.
    mp_reset_iter(proc->futexes);
    for (fernos_error_t err = mp_get_iter(proc->futexes, NULL, (void **)&fwq);
                err == FOS_E_SUCCESS; err = mp_next_iter(proc->futexes, NULL, (void **)&fwq)) {
        delete_wait_queue((wait_queue_t *)*fwq); 
    }

    // Finally clear the ol' map.
    mp_clear(proc->futexes);
}

process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    list_t *children = new_linked_list(al, sizeof(process_t *));
    list_t *zchildren = new_linked_list(al, sizeof(process_t *));
    id_table_t *thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    vector_wait_queue_t *join_queue = new_vector_wait_queue(al);
    vector_wait_queue_t *signal_queue = new_vector_wait_queue(al);
    map_t *futexes = new_chained_hash_map(al, sizeof(futex_t *), sizeof(basic_wait_queue_t *), 
            3, fm_key_eq, fm_key_hash);
    id_table_t *handle_table = new_id_table(al, FOS_MAX_HANDLES_PER_PROC);

    if (!proc || !children || !zchildren || !thread_table || !join_queue || 
            !signal_queue || !futexes || !handle_table) {
        al_free(al, proc);
        delete_list(children);
        delete_list(zchildren);
        delete_id_table(thread_table);
        delete_wait_queue((wait_queue_t *)signal_queue);
        delete_wait_queue((wait_queue_t *)join_queue);
        delete_map(futexes);
        delete_id_table(handle_table);

        return NULL;
    }

    proc->al = al;
    proc->exited = false;
    proc->exit_status = PROC_ES_UNSET;
    proc->pid = pid;
    proc->pd = pd;
    proc->parent = parent;

    proc->children = children;
    proc->zombie_children = zchildren;

    proc->thread_table = thread_table;
    proc->join_queue = join_queue;

    proc->main_thread = NULL;

    proc->sig_vec = empty_sig_vector();
    proc->sig_allow = empty_sig_vector();
    proc->signal_queue = signal_queue;

    proc->futexes = futexes;
    proc->handle_table = handle_table;

    const handle_t NULL_HANDLE = idtb_null_id(handle_table);
    proc->in_handle = NULL_HANDLE;
    proc->out_handle = NULL_HANDLE;

    return proc;
}

fernos_error_t new_process_fork(process_t *proc, thread_t *thr, proc_id_t cpid, process_t **out) {
    fernos_error_t err;

    if (!proc || !thr || !out) {
        return FOS_E_BAD_ARGS;
    }

    if (thr->proc != proc) {
        return FOS_E_BAD_ARGS;
    }

    phys_addr_t new_pd = copy_page_directory(proc->pd);
    if (new_pd == NULL_PHYS_ADDR) {
        return FOS_E_NO_MEM;
    }

    // Deciding not to use an iterator here because that technically mutates `proc`.
    for (thread_id_t tid = 0; tid < FOS_MAX_THREADS_PER_PROC;  tid++) {
        thread_t *parent_thread = idtb_get(proc->thread_table, tid);
        if (parent_thread && tid != thr->tid) {
            void *s = parent_thread->stack_base;
            const void *e = (const void *)FOS_THREAD_STACK_END(tid);

            // Free all stacks which aren't being used in the new process.
            pd_free_pages(new_pd, s, e);
        }
    }

    process_t *child = new_process(proc->al, cpid, new_pd, proc);

    if (!child) {
        delete_page_directory(new_pd);
        return FOS_E_NO_MEM;
    }

    // From this point on, `child` owns `new_pd`.
    // Deleting `child` will delete `new_pd`.

    // Now we must copy the given thread, add it to the thread table, and set it as the main thread!

    err = idtb_request_id(child->thread_table, thr->tid);
    thread_t *child_main_thr = NULL;

    if (err == FOS_E_SUCCESS) {
        // Place NULL in temporarily!
        //
        // This is a little confusing, but because we may end up calling the `delete_process` 
        // function below, we must make sure our child process is always able to be deleted 
        // correctly.
        //
        // Deleting a process will go through the thread table and delete all threads.
        // We reserved an ID in the thread table, but don't have a thread to place in it yet.
        // By setting to NULL here, we make sure the process destructor will succeed.
        // (Deleting NULL always succeeds!)
        idtb_set(child->thread_table, thr->tid, NULL);

        child_main_thr = new_thread_copy(thr, child);
    }

    if (err != FOS_E_SUCCESS || !child_main_thr) {
        delete_thread(child_main_thr);

        // The implications of a destructor returning an error are actually kinda annoying here.
        if (delete_process(child) != FOS_E_SUCCESS) {
            return FOS_E_ABORT_SYSTEM;
        }

        return FOS_E_NO_MEM;
    }

    // Here we successfully created a new main thread with a reserved thread ID!
    idtb_set(child->thread_table, child_main_thr->tid, child_main_thr);
    child->main_thread = child_main_thr;

    // From this point on, `child` owns `child_main_thread`.
    // Deleting `child` will delete everything created so far in this function
    // safely!

    child->in_handle = proc->in_handle;
    child->out_handle = proc->out_handle;

    // Ok, now actual handle copying will happen here too!

    for (id_t hid = 0; hid < FOS_MAX_HANDLES_PER_PROC; hid++) {
        handle_state_t *hs = idtb_get(proc->handle_table, hid);
        if (hs) {
            handle_state_t *hs_copy;

            err = idtb_request_id(child->handle_table, hid);
            if (err == FOS_E_SUCCESS) {
                // Same idea as what I said above about the thread table!
                idtb_set(child->handle_table, hid, NULL);
                err = copy_handle_state(hs, child, &hs_copy);
            }

            if (err != FOS_E_SUCCESS) {
                if (err == FOS_E_ABORT_SYSTEM) {
                    // In case of catastrophic error, don't worry about clean up.
                    return FOS_E_ABORT_SYSTEM;
                }

                // In case of other error, attempt cleanup.
                if (delete_process(child) != FOS_E_SUCCESS) {
                    return FOS_E_ABORT_SYSTEM;
                }

                return err;
            }

            // Success case! Place in table!
            idtb_set(child->handle_table, hid, hs_copy);
        }
    }

    *out = child;

    return FOS_E_SUCCESS;
}

fernos_error_t delete_process(process_t *proc) {
    if (!proc) {
        return FOS_E_SUCCESS; // allowed to delete NULL.
    }

    delete_page_directory(proc->pd);

    delete_list(proc->children);
    delete_list(proc->zombie_children);

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);
    idtb_reset_iterator(proc->thread_table);
    for (thread_id_t tid = idtb_get_iter(proc->thread_table);
            tid != NULL_TID; tid = idtb_next(proc->thread_table)) {
        thread_t *thr = idtb_get(proc->thread_table, tid);
        delete_thread(thr); // detaches thread if needed.
                            // We don't need to explicitly delete the stack of each thread because
                            // the page directory as a whole will be deleted in one go above!
    }

    delete_id_table(proc->thread_table);

    delete_wait_queue((wait_queue_t *)proc->join_queue);
    delete_wait_queue((wait_queue_t *)proc->signal_queue);

    // Delete all wait queues from the process futex map!
    proc_clear_fm_map(proc);

    // Now delete the map as a whole.
    delete_map(proc->futexes);

    // Delete each handle state individually.
    const handle_t NULL_HID = idtb_null_id(proc->handle_table);
    idtb_reset_iterator(proc->handle_table);
    for (handle_t hid = idtb_get_iter(proc->handle_table);
            hid != NULL_HID; hid = idtb_next(proc->handle_table)) {
        handle_state_t *hs = idtb_get(proc->handle_table, hid);
        fernos_error_t err = delete_handle_state(hs);
        if (err != FOS_E_SUCCESS) {
            return err; // Catastrophic (it's ok to exit early from the destructor
                        // given the system is about to lock up)
        }
    }

    // Delete table as a whole!
    delete_id_table(proc->handle_table);

    // Finally free entire process structure!
    al_free(proc->al, proc);

    return FOS_E_SUCCESS;
}

fernos_error_t proc_exec(process_t *proc, phys_addr_t new_pd, uintptr_t entry, uint32_t arg0,
        uint32_t arg1, uint32_t arg2) {
    fernos_error_t err;

    if (new_pd == NULL_PHYS_ADDR || !entry) {
        return FOS_E_BAD_ARGS;
    }

    thread_t *main_thr = proc->main_thread;
    if (!main_thr) {
        return FOS_E_STATE_MISMATCH;
    }

    // Now we start doing irreversible changes on `proc`!

    // First let's replace `proc->pd`.
    delete_page_directory(proc->pd);
    proc->pd = new_pd; // New pd should have no thread stacks allocated!

    // Now delete all non-main threads!

    for (thread_id_t tid = 0; tid < FOS_MAX_THREADS_PER_PROC; tid++) {
        thread_t *thr = (thread_t *)idtb_get(proc->thread_table, tid);
        if (thr && thr != main_thr) {
            delete_thread(thr); // This will deschedule/unwait as necessary!

            // No need to delete `thr`'s stack.
            // This was already done by just deleting the whole page directory!

            idtb_push_id(proc->thread_table, tid);
        }
    }

    // This will detach main thread too!
    thread_reset(main_thr, entry, arg0, arg1, arg2);

    // One point though, we are in a new page directory now, so we must manually reset stack_base.
    main_thr->stack_base = (void *)FOS_THREAD_STACK_END(main_thr->tid);

    proc->sig_vec = empty_sig_vector(); 
    proc->sig_allow = empty_sig_vector();

    // Delete all futexes and empty the map. (Futex's should really be moved to a 
    // plugin imo.
    proc_clear_fm_map(proc);

    // Now, delete all non-default handles, check for errors of course.
    for (handle_t h = 0; h < FOS_MAX_HANDLES_PER_PROC; h++) {
        handle_state_t *hs = idtb_get(proc->handle_table, h);
        if (hs && h != proc->in_handle && h != proc->out_handle) {
            err = delete_handle_state(hs);
            if (err != FOS_E_SUCCESS) {
                return FOS_E_ABORT_SYSTEM;
            }

            idtb_push_id(proc->handle_table, h);
        }
    }

    return FOS_E_SUCCESS;
}

thread_t *proc_new_thread(process_t *proc, uintptr_t entry, uint32_t arg0, uint32_t arg1,
        uint32_t arg2) {
    if (!proc || !entry) {
        return NULL;
    }

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    thread_id_t tid = idtb_pop_id(proc->thread_table);
    if (tid == NULL_TID) {
        return NULL;
    }

    // No more stack allocation here! Only ever by the page fault handler!
    thread_t *new_thr = new_thread(proc, tid, entry, arg0, arg1, arg2);
    if (!new_thr) {
        idtb_push_id(proc->thread_table, tid);
        return NULL;
    }

    idtb_set(proc->thread_table, tid, new_thr);

    if (!(proc->main_thread)) {
        proc->main_thread = new_thr;
    }

    return new_thr;
}

void proc_delete_thread(process_t *proc, thread_t *thr, bool return_stack) {
    if (!proc || !thr) {
        return;
    }

    if (thr->proc != proc) {
        return;
    }

    thread_id_t tid = thr->tid;

    if (return_stack) {
        void *tstack_start = thr->stack_base;
        const void *tstack_end = (void *)FOS_THREAD_STACK_END(tid);

        // Free the whole stack.
        pd_free_pages(proc->pd, tstack_start, tstack_end);
    }

    // detaches.
    delete_thread(thr);

    // Return the thread id so it can be used by later threads!
    idtb_push_id(proc->thread_table, tid);
}

fernos_error_t proc_register_futex(process_t *proc, futex_t *u_futex) {
    fernos_error_t err;

    if (!proc || !u_futex) {
        return FOS_E_BAD_ARGS;
    }

    // Is it already mapped?

    if (mp_get(proc->futexes, &u_futex)) {
        return FOS_E_ALREADY_ALLOCATED;
    }

    // Test out that we have access to the futex.

    futex_t test;
    err = mem_cpy_from_user(&test, proc->pd, u_futex, sizeof(futex_t), NULL);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // Create the wait queue.

    basic_wait_queue_t *bwq = new_basic_wait_queue(proc->al);
    if (!bwq) {
        return FOS_E_NO_MEM;
    }

    err = mp_put(proc->futexes, &u_futex, &bwq);
    if (err != FOS_E_SUCCESS) {
        delete_wait_queue((wait_queue_t *)bwq);
        return err;
    }
    
    return FOS_E_SUCCESS;
}

basic_wait_queue_t *proc_get_futex_wq(process_t *proc, futex_t *u_futex) {
    if (!proc || !u_futex) {
        return NULL;
    }

    basic_wait_queue_t **bwq = mp_get(proc->futexes, &u_futex);

    if (!bwq) {
        return NULL;
    }

    return *bwq;
}

void proc_deregister_futex(process_t *proc, futex_t *u_futex) {
    if (!proc || !u_futex) {
        return;
    }

    basic_wait_queue_t **bwq = mp_get(proc->futexes, &u_futex);

    if (!bwq) {
        return;
    }

    mp_remove(proc->futexes, &u_futex);

    delete_wait_queue((wait_queue_t *)*bwq);
}
