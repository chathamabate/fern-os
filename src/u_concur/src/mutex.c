
#include "u_concur/mutex.h"
#include "u_startup/syscall.h"

fernos_error_t init_mutex(mutex_t *mut) {
    if (!mut) {
        return FOS_E_BAD_ARGS;
    }

    *mut = MUT_UNLOCKED;

    return sc_futex_register(mut);
}

void cleanup_mutex(mutex_t *mut) {
    sc_futex_deregister(mut);
}

fernos_error_t mut_lock(mutex_t *mut) {
    fernos_error_t err;
    
    while (__sync_val_compare_and_swap(mut, MUT_UNLOCKED, MUT_LOCKED_UNCONTENDED) != MUT_UNLOCKED) {
        // While we fail to acquire the lock, wait on the lock?
        
        // We are going to wait, so first try to set the lock as contended.
        __sync_val_compare_and_swap(mut, MUT_LOCKED_UNCONTENDED, MUT_LOCKED_CONTENDED);

        // We only wait if the mutex was successfully set to contended!
        // (Remember, the comparison will safely happen inside the kernel)
        err = sc_futex_wait(mut, MUT_LOCKED_CONTENDED);
        if (err != FOS_E_SUCCESS) {
            return err; // fast fail if something went wrong with waiting on the lock.
        }
    }

    // We make it here means we successfully acuquired the mutex!

    return FOS_E_SUCCESS;
}

fernos_error_t mut_unlock(mutex_t *mut) {
    fernos_error_t err;

    if (__sync_val_compare_and_swap(mut, MUT_LOCKED_UNCONTENDED, MUT_UNLOCKED) == 
            MUT_LOCKED_UNCONTENDED) {
        return FOS_E_SUCCESS;
    }

    if (__sync_val_compare_and_swap(mut, MUT_LOCKED_CONTENDED, MUT_UNLOCKED) == 
            MUT_LOCKED_CONTENDED) {

        err = sc_futex_wake(mut, true);
        return err;
    }

    return FOS_E_STATE_MISMATCH;
}

