
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"

process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    if (!proc) {
        return NULL;
    }

    proc->al = al;
    proc->pid = pid;
    proc->parent = parent;

    proc->thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    if (!(proc->thread_table)) {
        al_free(al, proc);
        return NULL;
    }

    proc->main_thread = NULL;
    proc->pd = pd;

    return proc;
}

