
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

thread_t *new_thread(process_t *proc, thread_id_t tid, thread_entry_t entry, void *arg) {
    if (tid >= FOS_MAX_THREADS_PER_PROC) {
        return NULL;
    }
    
    // All threads must be owned by a parent process.
    if (!proc) {
        return NULL;
    }

    allocator_t *al = proc->al;

    // Many, many things to think about here tbh....

    thread_t *thr = al_malloc(al, sizeof(thread_t));
    if (!thr) {
        return NULL;
    }

    uint8_t *tstack_end = (uint8_t *)FOS_THREAD_STACK_END(tid);

    // NOTE: we used to allocate stack pages here... NOT ANYMORE!

    thr->state = THREAD_STATE_DETATCHED;

    thr->next_thread = NULL;
    thr->prev_thread = NULL;

    thr->tid = tid;
    thr->stack_base = NULL;

    *(process_t **)&(thr->proc) = proc;
    thr->wq = NULL;
    thr->u_wait_ctx = NULL;
    thr->exit_ret_val = NULL;

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

thread_t *new_thread_copy(thread_t *thr, process_t *new_proc) {
    if (!thr || !new_proc) {
        return NULL;
    }

    allocator_t *al = new_proc->al;

    thread_t *copy = al_malloc(al, sizeof(thread_t));
    if (!copy) {
        return NULL;
    }

    copy->state = THREAD_STATE_DETATCHED;
    copy->next_thread = NULL;
    copy->prev_thread = NULL;
    copy->tid = thr->tid;
    copy->stack_base = thr->stack_base;

    *(process_t **)&(copy->proc) = new_proc;
    copy->wq = NULL;
    copy->u_wait_ctx = NULL;

    mem_cpy(&(copy->ctx), &(thr->ctx), sizeof(user_ctx_t));
    copy->ctx.cr3 = new_proc->pd;
    
    copy->exit_ret_val = NULL;

    return copy;
}

void delete_thread(thread_t *thr) {
    if (!thr) {
        return;
    }

    al_free(thr->proc->al, thr);
}

