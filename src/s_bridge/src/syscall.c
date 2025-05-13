
#include "s_bridge/syscall.h"
#include "s_bridge/ctx.h"
#include "s_util/str.h"

void sc_thread_exit(void *retval) {
    (void)trigger_syscall(SCID_THREAD_EXIT, (uint32_t)retval, 0, 0, 0);

    // Does not make it here.
}

void sc_thread_sleep(uint32_t ticks) {
    (void)trigger_syscall(SCID_THREAD_SLEEP, ticks, 0, 0, 0);
}

fernos_error_t sc_thread_spawn(thread_id_t *tid, void *(*entry)(void *arg), void *arg) {
    return (fernos_error_t)trigger_syscall(SCID_THREAD_SPAWN, (uint32_t)tid, (uint32_t)entry, (uint32_t)arg, 0);
}

fernos_error_t sc_thread_join(join_vector_t jv, thread_id_t *joined, void **retval) {
    thread_join_ret_t join_ret;

    fernos_error_t err = trigger_syscall(SCID_THREAD_JOIN, jv, (uint32_t)&join_ret, 0, 0);

    if (joined) {
        *joined = join_ret.joined;
    }

    if (retval) {
        *retval = join_ret.retval;
    }

    return err;
}

fernos_error_t sc_futex_register(futex_t *futex) {
    return (fernos_error_t)trigger_syscall(SCID_FUTEX_REGISTER, (uint32_t)futex, 0, 0, 0);
}

void sc_futex_deregister(futex_t *futex) {
    (void)trigger_syscall(SCID_FUTEX_DEREGISTER, (uint32_t)futex, 0, 0, 0);
}

fernos_error_t sc_futex_wait(futex_t *futex, futex_t exp_val) {
    return (fernos_error_t)trigger_syscall(SCID_FUTEX_WAIT, (uint32_t)futex, exp_val, 0, 0);
}

fernos_error_t sc_futex_wake(futex_t *futex, bool all) {
    return (fernos_error_t)trigger_syscall(SCID_FUTEX_WAKE, (uint32_t)futex, all, 0, 0);
}

void sc_term_put_s(const char *s) {
    (void)trigger_syscall(SCID_TERM_PUT_S, (uint32_t)s, str_len(s) + 1, 0, 0);
}

