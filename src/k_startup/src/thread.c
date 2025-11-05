
#include "k_startup/thread.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/gdt.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"
#include "k_startup/process.h"
#include "s_util/str.h"
#include "u_startup/main.h"
#include "s_util/str.h"

thread_t *new_thread(process_t *proc, thread_id_t tid, uintptr_t entry, 
        uint32_t arg0, uint32_t arg1, uint32_t arg2) {
    if (tid >= FOS_MAX_THREADS_PER_PROC) {
        return NULL;
    }
    
    // All threads must be owned by a parent process.
    if (!proc) {
        return NULL;
    }

    allocator_t *al = proc->al;

    thread_t *thr = al_malloc(al, sizeof(thread_t));
    if (!thr) {
        return NULL;
    }

    uint8_t *tstack_end = (uint8_t *)FOS_THREAD_STACK_END(tid);

    // NOTE: we used to allocate stack pages here... NOT ANYMORE!

    init_ring_element(&(thr->super));

    thr->state = THREAD_STATE_DETATCHED;

    thr->tid = tid;
    thr->stack_base = tstack_end; // New change here! hopefully this works!

    *(process_t **)&(thr->proc) = proc;
    thr->wq = NULL;
    mem_set(thr->wait_ctx, 0, sizeof(thr->wait_ctx));
    thr->exit_ret_val = NULL;

    thr->ctx = (user_ctx_t) {
        .ds = USER_DATA_SELECTOR,
        .cr3 = proc->pd,

        .eax = entry,
        .ebx = arg0,
        .ecx = arg1,
        .edx = arg2,

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

    init_ring_element(&(copy->super));
    copy->state = THREAD_STATE_DETATCHED;
    copy->tid = thr->tid;
    copy->stack_base = thr->stack_base;

    *(process_t **)&(copy->proc) = new_proc;
    copy->wq = NULL;
    mem_set(copy->wait_ctx, 0, sizeof(copy->wait_ctx));

    mem_cpy(&(copy->ctx), &(thr->ctx), sizeof(user_ctx_t));
    copy->ctx.cr3 = new_proc->pd;
    
    copy->exit_ret_val = NULL;

    return copy;
}

void thread_detach(thread_t *thr) {
    if (thr->state == THREAD_STATE_WAITING) {
        wq_remove(thr->wq, thr);
        thr->wq = NULL;
        mem_set(thr->wait_ctx, 0, sizeof(thr->wait_ctx));

        thr->state = THREAD_STATE_DETATCHED;
    } else if (thr->state == THREAD_STATE_SCHEDULED) {
        ring_element_detach((ring_element_t *)thr);
        thr->state = THREAD_STATE_DETATCHED;
    }
}

void thread_schedule(thread_t *thr, ring_t *r) {
    if (thr->state != THREAD_STATE_DETATCHED) {
        thread_detach(thr);
    }

    ring_element_attach((ring_element_t *)thr, r);

    thr->state = THREAD_STATE_SCHEDULED;
}

void delete_thread(thread_t *thr) {
    if (!thr) {
        return;
    }

    thread_detach(thr);

    al_free(thr->proc->al, thr);
}

