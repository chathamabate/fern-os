
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/thread.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"

process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    id_table_t *thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    vector_wait_queue_t *join_queue = new_vector_wait_queue(al);

    if (!proc || !thread_table || !join_queue) {
        al_free(al, proc);
        delete_id_table(thread_table);
        delete_wait_queue((wait_queue_t *)join_queue);

        return NULL;
    }

    proc->al = al;
    proc->pid = pid;
    proc->parent = parent;

    proc->thread_table = thread_table;
    proc->join_queue = join_queue;

    proc->main_thread = NULL;
    proc->pd = pd;

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


// Maybe we defer the process deletion stuff out to the kernel state??
// That's def an idea...
//
// When a process thread exits??
// OK, god only knows how this is all going to work tbh...
