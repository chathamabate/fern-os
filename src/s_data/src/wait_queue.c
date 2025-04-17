
#include "s_data/wait_queue.h"
#include "s_data/list.h"

static void delete_basic_wait_queue(wait_queue_t *wq);
static void bwq_remove(wait_queue_t *wq, void *item);

static const wait_queue_impl_t BWQ_IMPL = {
    .delete_wait_queue = delete_basic_wait_queue,
    .wq_remove = bwq_remove,
    .wq_dump = NULL // Maybe implement one day.
};

basic_wait_queue_t *new_basic_wait_queue(allocator_t *al) {

}
    
static void delete_basic_wait_queue(wait_queue_t *wq) {

}

fernos_error_t bwq_enqueue(basic_wait_queue_t *bwq, void *item) {
    
}

fernos_error_t bwq_notify(basic_wait_queue_t *bwq, bwq_notify_mode_t mode) {

}

fernos_error_t bwq_pop(basic_wait_queue_t *bwq, void **item) {

}

static void bwq_remove(wait_queue_t *wq, void *item) {

}
