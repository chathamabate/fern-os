
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_startup/wait_queue.h"

struct _process_t {
    /**
     * Like in linux, signals will be one form of IPC.
     */
    uint32_t sig_vec;

    /**
     * By default, a signal of any type will kill this process.
     *
     * If you'd like a signal to be passed to the signal wait queue, make sure it is marked 
     * allowed via this mask. `1` means allowed, `0` means kill this process.
     */
    uint32_t sig_allow;

    /**
     * When one or more signals arrive, and they're all marked as allow, the signal vector will be
     * passed to this queue. 
     *
     * This queue will deal with waking up the correct threads and editing the signal vector 
     * to reflect which signals were handled.
     *
     * This queue will likely ONLY be notified when new signals arrive. So, before adding a thread 
     * to this queue, you may want to check the signal vector first to see if no waiting is 
     * necessary.
     */
    sig_wait_queue_t *swq;

    /**
     * OK, now what, conditions???
     */
    
};
