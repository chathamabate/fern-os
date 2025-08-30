
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/thread.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"
#include "s_util/str.h"

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

process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    list_t *children = new_linked_list(al, sizeof(process_t *));
    list_t *zchildren = new_linked_list(al, sizeof(process_t *));
    id_table_t *thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    vector_wait_queue_t *join_queue = new_vector_wait_queue(al);
    vector_wait_queue_t *signal_queue = new_vector_wait_queue(al);
    map_t *futexes = new_chained_hash_map(al, sizeof(futex_t *), sizeof(basic_wait_queue_t *), 
            3, fm_key_eq, fm_key_hash);
    id_table_t *file_handle_table = new_id_table(al, FOS_MAX_FILE_HANDLES_PER_PROC);

    if (!proc || !children || !zchildren || !thread_table || !join_queue || 
            !signal_queue || !futexes || !file_handle_table) {
        al_free(al, proc);
        delete_list(children);
        delete_list(zchildren);
        delete_id_table(thread_table);
        delete_wait_queue((wait_queue_t *)signal_queue);
        delete_wait_queue((wait_queue_t *)join_queue);
        delete_map(futexes);
        delete_id_table(file_handle_table);

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

    proc->file_handle_table = file_handle_table;

    return proc;
}

process_t *new_process_fork(process_t *proc, thread_t *thr, proc_id_t cpid) {
    fernos_error_t err;

    if (!proc || !thr) {
        return NULL;
    }

    if (thr->proc != proc) {
        return NULL;
    }

    phys_addr_t new_pd = copy_page_directory(proc->pd);
    if (new_pd == NULL_PHYS_ADDR) {
        return NULL;
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
        return NULL;
    }


    // From this point on, `child` owns `new_pd`.
    // Deleting `child` will delete `new_pd`.

    // Now we must copy the given thread, add it to the thread table, and set it as the main thread!

    err = idtb_request_id(child->thread_table, thr->tid);
    thread_t *child_main_thr = new_thread_copy(thr, child);

    if (err != FOS_SUCCESS || !child_main_thr) {
        delete_process(child);
        delete_thread(child_main_thr);

        return NULL;
    }

    idtb_set(child->thread_table, child_main_thr->tid, child_main_thr);
    child->main_thread = child_main_thr;

    // Finally, let's copy over newly allocated file handle states!
    // Kinda like with threads, each file state copied will need the same file handle
    // in the forked process!

    for (file_handle_t fh = 0; fh < FOS_MAX_FILE_HANDLES_PER_PROC; fh++) {
        file_handle_state_t *fh_state = idtb_get(proc->file_handle_table, fh);
        if (fh_state) {
            err = idtb_request_id(child->file_handle_table, fh);
            file_handle_state_t *fh_state_copy = NULL;

            if (err == FOS_SUCCESS) {
                fh_state_copy = al_malloc(proc->al, sizeof(file_handle_state_t));
                if (fh_state_copy) {
                    mem_cpy(fh_state_copy, fh_state, sizeof(file_handle_state_t));

                    idtb_set(child->file_handle_table, fh, fh_state_copy);
                }
            }

            if (err != FOS_SUCCESS || !fh_state_copy) {
                delete_process(child);
                return NULL;
            }
        }
    }

    return child;
}

void delete_process(process_t *proc) {
    if (!proc) {
        return;
    }

    delete_page_directory(proc->pd);

    delete_list(proc->children);
    delete_list(proc->zombie_children);

    // Dangerously delete all threads.
    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);
    idtb_reset_iterator(proc->thread_table);
    for (thread_id_t tid = idtb_get_iter(proc->thread_table);
            tid != NULL_TID; tid = idtb_next(proc->thread_table)) {
        thread_t *thr = idtb_get(proc->thread_table, tid);
        delete_thread(thr);
    }

    delete_id_table(proc->thread_table);

    delete_wait_queue((wait_queue_t *)proc->join_queue);
    delete_wait_queue((wait_queue_t *)proc->signal_queue);

    // Now we need to delete every wait queue (If there are any)
    basic_wait_queue_t **fwq; // Pretty confusing, but yes this should be a **.
    mp_reset_iter(proc->futexes);
    for (fernos_error_t err = mp_get_iter(proc->futexes, NULL, (void **)&fwq);
                err == FOS_SUCCESS; err = mp_next_iter(proc->futexes, NULL, (void **)&fwq)) {
        delete_wait_queue((wait_queue_t *)*fwq); 
    }

    // Now delete the map as a whole.
    delete_map(proc->futexes);

    // Now delete all file handle states.

    const file_handle_t NULL_FH = idtb_null_id(proc->file_handle_table);
    idtb_reset_iterator(proc->file_handle_table);
    for (file_handle_t fh = idtb_get_iter(proc->file_handle_table); fh != NULL_FH;
            fh = idtb_next(proc->file_handle_table)) {
        file_handle_state_t *fh_state = idtb_get(proc->file_handle_table, fh);
        al_free(proc->al, fh_state);
    }

    delete_id_table(proc->file_handle_table);

    // Finally free entire process structure!
    al_free(proc->al, proc);
}

/**
 * Allocate a new thread, and its initial stack pages.
 */
static thread_t *proc_new_thread_with_stack(process_t *proc,
        thread_id_t tid, thread_entry_t entry, void *arg) {
    thread_t *new_thr = new_thread(proc, tid, entry, arg);
    if (!new_thr) {
        return NULL;
    }

    const void *true_e;
    
    const void *tstack_end = (void *)FOS_THREAD_STACK_END(tid);
    void *tstack_start = (void *)((uint8_t *)tstack_end - M_4K);

    new_thr->stack_base = (void *)tstack_end;

    fernos_error_t err = pd_alloc_pages(proc->pd, true, tstack_start, tstack_end, &true_e);

    if (err != FOS_SUCCESS && err != FOS_ALREADY_ALLOCATED) {
        delete_thread(new_thr);
        return NULL;
    }

    // Only set the new stack base one the allocation succeeds. 
    new_thr->stack_base = tstack_start;

    return new_thr;
}

thread_t *proc_new_thread(process_t *proc, thread_entry_t entry, void *arg) {
    if (!proc || !entry) {
        return NULL;
    }

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    thread_id_t tid = idtb_pop_id(proc->thread_table);
    if (tid == NULL_TID) {
        return NULL;
    }

    thread_t *new_thr = proc_new_thread_with_stack(proc, tid, entry, arg);
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

    delete_thread(thr);

    // Return the thread id so it can be used by later threads!
    idtb_push_id(proc->thread_table, tid);
}

fernos_error_t proc_register_futex(process_t *proc, futex_t *u_futex) {
    fernos_error_t err;

    if (!proc || !u_futex) {
        return FOS_BAD_ARGS;
    }

    // Is it already mapped?

    if (mp_get(proc->futexes, &u_futex)) {
        return FOS_ALREADY_ALLOCATED;
    }

    // Test out that we have access to the futex.

    futex_t test;
    err = mem_cpy_from_user(&test, proc->pd, u_futex, sizeof(futex_t), NULL);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Create the wait queue.

    basic_wait_queue_t *bwq = new_basic_wait_queue(proc->al);
    if (!bwq) {
        return FOS_NO_MEM;
    }

    err = mp_put(proc->futexes, &u_futex, &bwq);
    if (err != FOS_SUCCESS) {
        delete_wait_queue((wait_queue_t *)bwq);
        return err;
    }
    
    return FOS_SUCCESS;
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

fernos_error_t proc_register_file_handle(process_t *proc, fs_node_key_t nk, file_handle_t *fh) {
    if (!fh || !nk) {
        return FOS_BAD_ARGS;
    }

    file_handle_state_t *new_state = al_malloc(proc->al, sizeof(file_handle_state_t));
    if (!new_state) {
        return FOS_NO_MEM;
    }

    *(fs_node_key_t *)&(new_state->nk) = nk;
    new_state->pos = 0;

    const file_handle_t NULL_FH = idtb_null_id(proc->file_handle_table);
    file_handle_t new_fh = idtb_pop_id(proc->file_handle_table);
    if (new_fh == NULL_FH) {
        al_free(proc->al, new_state);
        return FOS_EMPTY;
    }

    idtb_set(proc->file_handle_table, new_fh, new_state);

    *fh = new_fh;

    return FOS_SUCCESS;
}

void proc_deregister_file_handle(process_t *proc, file_handle_t fh) {
    file_handle_state_t *fh_state = idtb_get(proc->file_handle_table, fh);
    if (fh_state) {
        al_free(proc->al, fh_state);
        idtb_push_id(proc->file_handle_table, fh);
    }
}
