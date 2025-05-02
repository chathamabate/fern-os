
#include "s_bridge/syscall.h"
#include "s_bridge/ctx.h"
#include "s_util/str.h"

void sc_thread_exit(void *retval) {
    (void)trigger_syscall(SCID_THREAD_EXIT, retval);

    // Does not make it here.
}

void sc_thread_sleep(uint32_t ticks) {
    (void)trigger_syscall(SCID_THREAD_SLEEP, (void *)ticks);
}

fernos_error_t sc_thread_spawn(thread_id_t *tid, void *(*entry)(void *arg), void *arg) {
    thread_spawn_arg_t ts_arg = {
        .tid = tid,

        .entry = entry,
        .arg = arg
    };

    return (fernos_error_t)trigger_syscall(SCID_THREAD_SPAWN, &ts_arg);
}

fernos_error_t sc_thread_join(join_vector_t jv, thread_id_t *joined, void **retval) {
    thread_join_ret_t join_ret;

    thread_join_arg_t tj_arg = {
        .jv = jv,
        .join_ret = &join_ret
    };

    fernos_error_t err = trigger_syscall(SCID_THREAD_JOIN, &tj_arg);

    if (joined) {
        *joined = join_ret.joined;
    }

    if (retval) {
        *retval = join_ret.retval;
    }

    return err;
}

void sc_term_put_s(const char *s) {
    buffer_arg_t arg = {
        .buf_size = str_len(s) + 1,
        .buf = (void *)s
    };

    (void)trigger_syscall(SCID_TERM_PUT_S, &arg);
}

