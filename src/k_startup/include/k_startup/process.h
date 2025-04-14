
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_startup/wait_queue.h"
#include "s_data/id_table.h"

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
     * Conditions will be local to each process. A thread from one process can NEVER wait on a 
     * condition from another process.
     *
     * This is essentially a map<Local Condition ID, cond_wait_queue_t *>
     *
     * If a condition is deleted while threads are waiting, all threads should be woken up with 
     * some sort of status code.
     */
    id_table_t *conds;

    // How do we know when a child process has died (A signal, ok, but where does it's pid/pointer
    // get stored?? A Hashset of living and dead children??
    // When we do reap, how do we know who's died??
    // A linked list maybe?? Not the worst thing in the world??
    // A table could be better IMO... a probed table maybe??
    // Do I really need to redo all of these data structures??
    // ... Probably tbh...
    // A linked list wouldn't be so bad tbh...
    // Probs will need it for the wait_queue anyway IMO...
};
