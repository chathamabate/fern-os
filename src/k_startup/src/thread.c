
#include "k_startup/thread.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/gdt.h"
#include "s_data/wait_queue.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"
#include "k_startup/process.h"
#include "s_util/err.h"
#include "s_util/str.h"
#include "u_startup/main.h"
#include "k_startup/page.h"
#include "k_bios_term/term.h"

thread_t *new_thread(allocator_t *al, thread_id_t tid, process_t *proc, thread_entry_t entry, void *arg) {
    if (tid >= FOS_MAX_THREADS_PER_PROC) {
        return NULL;
    }
    
    // All threads must be owned by a parent process.
    if (!proc) {
        return NULL;
    }

    thread_t *thr = al_malloc(al, sizeof(thread_t));
    if (!thr) {
        return NULL;
    }

    // All thread stacks must be given at least 1 page to start!

    const void *true_e;
    uint8_t *tstack_end = (uint8_t *)FOS_THREAD_STACK_END(tid);
    fernos_error_t err = pd_alloc_pages(proc->pd, true, tstack_end - (2*M_4K), tstack_end, &true_e);

    if (err != FOS_SUCCESS && err != FOS_ALREADY_ALLOCATED) {
        al_free(al, thr);
        return NULL;
    }

    thr->al = al;
    thr->state = THREAD_STATE_DETATCHED;

    thr->next_thread = NULL;
    thr->prev_thread = NULL;

    thr->tid = tid;

    thr->proc = proc;
    thr->wq = NULL;

    thr->ctx = (user_ctx_t) {
        .ds = USER_DATA_SELECTOR,
        .cr3 = proc->pd,

        .eax = (uint32_t)entry,
        .ecx = (uint32_t)arg,

        .eip = (uint32_t)thread_entry_routine,
        .cs = USER_CODE_SELECTOR,
        .eflags = read_eflags() | (1 << 9),

        .esp = (uint32_t)tstack_end,
        .ss = USER_DATA_SELECTOR
    };

    return thr;
}

void delete_thread(thread_t *thr) {
    if (!thr) {
        return;
    }

    if (thr->state == THREAD_STATE_WAITING) {
        wq_remove(thr->wq, thr);
    } else if (thr->state == THREAD_STATE_SCHEDULED) {
        // Remember, cyclic!
        thr->prev_thread->next_thread = thr->next_thread; 
        thr->next_thread->prev_thread = thr->prev_thread;

        thr->prev_thread = NULL;
        thr->next_thread = NULL;
    }
    
    void *tstack_start = (void *)FOS_THREAD_STACK_START(thr->tid);
    const void *tstack_end = (void *)FOS_THREAD_STACK_END(thr->tid);

    // Free the whole stack.
    pd_free_pages(thr->proc->pd, tstack_start, tstack_end);

    // Free the thread structure.
    al_free(thr->al, thr);
}

