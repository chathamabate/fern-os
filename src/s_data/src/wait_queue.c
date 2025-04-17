
#include "s_data/wait_queue.h"
#include "s_data/list.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"

/*
 * Basic Wait Queue.
 */

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

        return NULL;
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
        if (item) {
            *item = NULL;
        }
        return FOS_EMPTY;
    }

    return l_pop_front(bwq->ready_q, item);
}

/**
 * Remove all occurences of the pointer item from the list l.
 */
static void bwq_l_remove(list_t *l, void *item) {
    l_reset_iter(l);
    void **iter = l_get_iter(l);
    while (iter) {
        if (*iter == item) {
            l_pop_iter(l, NULL);
            iter = l_get_iter(l);
        } else {
            iter = l_next_iter(l);
        }
    }
}

static void bwq_remove(wait_queue_t *wq, void *item) {
    basic_wait_queue_t *bwq = (basic_wait_queue_t *)wq;

    bwq_l_remove(bwq->wait_q, item);
    bwq_l_remove(bwq->ready_q, item);
}

/*
 * Vector Wait Queue.
 */

static void delete_vector_wait_queue(wait_queue_t *wq);
static void vwq_remove(wait_queue_t *wq, void *item);

static const wait_queue_impl_t VWQ_IMPL = {
    .delete_wait_queue = delete_vector_wait_queue,
    .wq_remove = vwq_remove,
    .wq_dump = NULL // Maybe implement one day.
};

vector_wait_queue_t *new_vector_wait_queue(allocator_t *al) {
    vector_wait_queue_t *vwq = al_malloc(al, sizeof(vector_wait_queue_t));
    list_t *wq = new_linked_list(al, sizeof(vwq_wait_pair_t));
    list_t *rq =  new_linked_list(al, sizeof(vwq_ready_pair_t));

    if (!vwq || !wq || !rq) {
        al_free(al, vwq);

        delete_list(wq);
        delete_list(rq);

        return NULL;
    }

    *(const wait_queue_impl_t **)&(vwq->super.impl) = &VWQ_IMPL;
    vwq->al = al;
    vwq->wait_q = wq;
    vwq->ready_q = rq;

    return vwq;
}

static void delete_vector_wait_queue(wait_queue_t *wq) {
    vector_wait_queue_t *vwq = (vector_wait_queue_t *)wq;

    delete_list(vwq->wait_q);
    delete_list(vwq->ready_q);
    al_free(vwq->al, vwq);
}

fernos_error_t vwq_enqueue(vector_wait_queue_t *vwq, void *item, uint32_t ready_mask) {
    if (ready_mask == 0) {
        return FOS_BAD_ARGS;
    }

    vwq_wait_pair_t wp = {
        .item = item,
        .ready_mask = ready_mask
    };

    return l_push_back(vwq->wait_q, &wp);
}

fernos_error_t vwq_notify(vector_wait_queue_t *vwq, uint8_t ready_id, vwq_notify_mode_t mode) {
    if (ready_id >= 32) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err = FOS_SUCCESS;
    
    vwq_wait_pair_t *iter;
    vwq_ready_pair_t rp;
    uint32_t mask = 1 << ready_id;

    switch (mode) {
    case VWQ_NOTIFY_FIRST:
        l_reset_iter(vwq->wait_q);
        for (iter = l_get_iter(vwq->wait_q); iter != NULL; iter = l_next_iter(vwq->wait_q)) {
            if (iter->ready_mask & mask) {
                rp = (vwq_ready_pair_t) {
                    .item = iter->item,
                    .ready_id = ready_id
                };

                err = l_push_back(vwq->ready_q, &rp); 
                if (err == FOS_SUCCESS) {
                    l_pop_iter(vwq->wait_q, NULL);
                }

                // Here we exit after the first iterator match.
                break;
            }
        }
        break;

    case VWQ_NOTIFY_ALL:
        l_reset_iter(vwq->wait_q);
        for (iter = l_get_iter(vwq->wait_q); err == FOS_SUCCESS && iter != NULL; ) {

            if (iter->ready_mask & mask) {
                rp = (vwq_ready_pair_t) {
                    .item = iter->item,
                    .ready_id = ready_id
                };

                err = l_push_back(vwq->ready_q, &rp); 
                if (err == FOS_SUCCESS) {
                    l_pop_iter(vwq->wait_q, NULL);
                    iter = l_get_iter(vwq->wait_q);
                }
            } else {
                iter = l_next_iter(vwq->wait_q);
            }

            // Here we just keep going. (Unless there's an error)
        }
        break;

    default:
        break;
    }

    return err;
}

fernos_error_t vwq_pop(vector_wait_queue_t *vwq, void **item, uint8_t *ready_id) {
    vwq_ready_pair_t rp = {
        .item = NULL,
        .ready_id = VWQ_NULL_READY_ID
    };

    fernos_error_t err;

    if (l_get_len(vwq->ready_q) > 0) {
        l_pop_front(vwq->ready_q, &rp);
        err = FOS_SUCCESS;
    } else {
        err = FOS_EMPTY;
    }

    if (item) {
        *item = rp.item;
    }

    if (ready_id) {
        *ready_id = rp.ready_id;
    }

    return err;
}

static void vwq_remove(wait_queue_t *wq, void *item) {
    vector_wait_queue_t *vwq = (vector_wait_queue_t *)wq;

    l_reset_iter(vwq->wait_q);
    vwq_wait_pair_t *wp_iter = l_get_iter(vwq->wait_q);
    while (wp_iter) {
        if (wp_iter->item == item) {
            l_pop_iter(vwq->wait_q, NULL);
            wp_iter = l_get_iter(vwq->wait_q);
        } else {
            wp_iter = l_next_iter(vwq->wait_q);
        }
    }

    l_reset_iter(vwq->ready_q);
    vwq_ready_pair_t *rp_iter = l_get_iter(vwq->ready_q);
    while (rp_iter) {
        if (rp_iter->item == item) {
            l_pop_iter(vwq->ready_q, NULL);
            rp_iter = l_get_iter(vwq->ready_q);
        } else {
            rp_iter = l_next_iter(vwq->ready_q);
        }
    }
}
