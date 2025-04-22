
#include "k_startup/process.h"
#include "k_startup/fwd_defs.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"

process_t *new_blank_process(allocator_t *al) {
    process_t *proc = al_malloc(al, sizeof(process_t));
    if (!proc) {
        return NULL;
    }

    proc->al = al;
    proc->pid = FOS_MAX_PROCS;
    proc->parent = NULL;

    proc->thread_table = new_id_table(al, FOS_MAX_THREADS_PER_PROC);
    if (!(proc->thread_table)) {
        al_free(al, proc);
        return NULL;
    }

    proc->main_thread = NULL;
    proc->pd = NULL_PHYS_ADDR;

    return proc;
}

