
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/thread.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"

#include "k_startup/page.h"
#include "k_startup/page_helpers.h"

static bool fm_key_eq(chained_hash_map_t *futexes, const void *k0, const void *k1) {
    (void)futexes;
    return *(const futex_t **)k0 == *(const futex_t **)k1;
}

static uint32_t fm_key_hash(chained_hash_map_t *futexes, const void *k) {
    (void)futexes;

    const futex_t *futex = *(const futex_t **)k;

    // Kinda just a random hash function here.
    return (((uint32_t)futex + 1373) * 7) + 2;
}

process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    id_table_t *thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    vector_wait_queue_t *join_queue = new_vector_wait_queue(al);
    map_t *futexes = new_chained_hash_map(al, sizeof(futex_t *), sizeof(basic_wait_queue_t *), 
            3, fm_key_eq, fm_key_hash);

    if (!proc || !thread_table || !join_queue || !futexes) {
        al_free(al, proc);
        delete_id_table(thread_table);
        delete_wait_queue((wait_queue_t *)join_queue);
        delete_map(futexes);

        return NULL;
    }

    proc->al = al;
    proc->pid = pid;
    proc->parent = parent;

    proc->thread_table = thread_table;
    proc->join_queue = join_queue;

    proc->main_thread = NULL;
    proc->pd = pd;

    proc->futexes = futexes;

    return proc;
}

fernos_error_t proc_create_thread(process_t *proc, thread_t **thr, 
        thread_entry_t entry, void *arg) {
    if (!thr || !entry) {
        return FOS_BAD_ARGS;
    }

    thread_id_t tid = idtb_pop_id(proc->thread_table);

    if (tid == idtb_null_id(proc->thread_table)) {
        *thr = NULL;

        return FOS_NO_MEM;
    }

    thread_t *new_thr = new_thread(proc, tid, entry, arg); 

    if (!new_thr) {
        idtb_push_id(proc->thread_table, tid);
        *thr = NULL;
        
        return FOS_NO_MEM;
    }

    idtb_set(proc->thread_table, tid, new_thr);

    if (!(proc->main_thread)) {
        proc->main_thread = new_thr;
    }

    *thr = new_thr;

    return FOS_SUCCESS;
}

void proc_reap_thread(process_t *proc, thread_t *thr, bool return_stack) {
    if (!proc || !thr || thr->state != THREAD_STATE_EXITED) {
        return;
    }

    thread_id_t tid = thr->tid;

    reap_thread(thr, return_stack);

    // Return the thread id so it can be used by later threads!
    idtb_push_id(proc->thread_table, tid);
}

fernos_error_t proc_fork(process_t *proc, thread_t *thr, proc_id_t pid, process_t **new_proc) {
    if (!proc || !thr || !new_proc) {
        return FOS_BAD_ARGS;
    }

    if (thr->proc != proc) {
        return FOS_BAD_ARGS;
    }

    phys_addr_t new_pd = copy_page_directory(proc->pd);
    if (new_pd == NULL_PHYS_ADDR) {
        return FOS_NO_MEM;
    }

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    idtb_reset_iterator(proc->thread_table);

    // Free all stacks which were used in the parent process, but will NOT be used at first
    // in this child process.
    for (thread_id_t tid = idtb_get_iter(proc->thread_table); tid != NULL_TID; 
            tid = idtb_next(proc->thread_table)) {
        if (tid != thr->tid) {
            void *s = (void *)FOS_THREAD_STACK_START(tid);
            const void *e = (const void *)FOS_THREAD_STACK_END(tid);

            pd_free_pages(new_pd, s, e);
        }
    }

    // Now we create a new process object, and copy the given thread over.
    // Ooob I am really out of focus today tbh...
    
    process_t *child = new_process(proc->al, pid, new_pd, proc);

    if (!child) {
        // TODO do something here.
    }

    thread_t *child_main_thr = thr_copy()


}


