
#include "s_data/wait_queue.h"
#include "s_data/list.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"

static void delete_basic_wait_queue(wait_queue_t *wq);
static void bwq_remove(wait_queue_t *wq, void *item);

static const wait_queue_impl_t BWQ_IMPL = {
    .delete_wait_queue = delete_basic_wait_queue,
    .wq_remove = bwq_remove,
    .wq_dump = NULL // Maybe implement one day.
};

basic_wait_queue_t *new_basic_wait_queue(allocator_t *al) {
    basic_wait_queue_t *bwq = al_malloc(al, sizeof(basic_wait_queue_t));
    list_t *wq = new_linked_list(al, sizeof(void *));
    list_t *rq = new_linked_list(al, sizeof(void *));

    if (!bwq || !rq || !wq) {
        // This strategy relies on all destructor like functions being able to receive NULL.
        al_free(al, bwq);
        delete_list(rq);
        delete_list(wq);
    }

    // We also need to allocate two lists bruh.

    *(const wait_queue_impl_t **)&(bwq->super.impl) = &BWQ_IMPL;
    bwq->al = al;
    bwq->wait_q = wq;
    bwq->ready_q = rq;

    return bwq;
}
    
static void delete_basic_wait_queue(wait_queue_t *wq) {
    basic_wait_queue_t *bwq = (basic_wait_queue_t *)wq;

    delete_list(bwq->wait_q);
    delete_list(bwq->ready_q);
    al_free(bwq->al, bwq);
}

fernos_error_t bwq_enqueue(basic_wait_queue_t *bwq, void *item) {
    return l_push_back(bwq->wait_q, &item);
}

fernos_error_t bwq_notify(basic_wait_queue_t *bwq, bwq_notify_mode_t mode) {
    fernos_error_t res = FOS_SUCCESS;

    switch (mode) {

    case BWQ_NOTIFY_NEXT:
        if (l_get_len(bwq->wait_q) > 0) {
            // Since we know we have a non-zero length, this call should always succeed.
            void *item = *(void **)l_get_ptr(bwq->wait_q, 0);

            // This call may fail due to memory problems.
            res = l_push_back(bwq->ready_q, &item);
            if (res == FOS_SUCCESS) {
                l_pop_front(bwq->wait_q, NULL);
            }
        }
        break;

    case BWQ_NOTIFY_LAST:
        if (l_get_len(bwq->wait_q) > 0) {
            void *item = *(void **)l_get_ptr(bwq->wait_q, l_get_len(bwq->wait_q) - 1);

            res = l_push_back(bwq->ready_q, &item);
            if (res == FOS_SUCCESS) {
                l_pop_back(bwq->wait_q, NULL);
            }
        }
        break;

    case BWQ_NOTIFY_ALL:
        while (res == FOS_SUCCESS && l_get_len(bwq->wait_q) > 0) {
            // We exit this loop once all items have been transfered to the ready queue.
            // OR once a single transfer from the wait q to the ready q fails.

            void *item = *(void **)l_get_ptr(bwq->wait_q, 0);
            res = l_push_back(bwq->ready_q, &item);
            if (res == FOS_SUCCESS) {
                l_pop_front(bwq->wait_q, NULL);
            }         
        }
        break;

    default:
        // If you give an invalid code, let's just do nothing. (Might change this later)
        break;
    }

    return res;
}

fernos_error_t bwq_pop(basic_wait_queue_t *bwq, void **item) {
    if (l_get_len(bwq->ready_q) == 0) {
        return FOS_EMPTY;
    }

    return l_pop_front(bwq->ready_q, item);
}

static void bwq_remove(wait_queue_t *wq, void *item) {

}
