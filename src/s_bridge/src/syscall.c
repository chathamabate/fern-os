
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

void sc_term_put_s(const char *s) {
    buffer_arg_t arg = {
        .buf_size = str_len(s) + 1,
        .buf = (void *)s
    };

    (void)trigger_syscall(SCID_TERM_PUT_S, &arg);
}

