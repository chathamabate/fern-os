
#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_bridge/ctx.h"
#include "s_util/str.h"

fernos_error_t sc_proc_fork(proc_id_t *cpid) {
    return (fernos_error_t)trigger_syscall(SCID_PROC_FORK, (uint32_t)cpid, 0, 0, 0);
}

void sc_proc_exit(proc_exit_status_t status) {
    (void)trigger_syscall(SCID_PROC_EXIT, (uint32_t)status, 0, 0, 0);
}

fernos_error_t sc_proc_reap(proc_id_t cpid, proc_id_t *rcpid, proc_exit_status_t *rces) {
    return (fernos_error_t)trigger_syscall(SCID_PROC_REAP, (uint32_t)cpid, (uint32_t)rcpid, 
            (uint32_t)rces, 0);
}

fernos_error_t sc_signal(proc_id_t pid, sig_id_t sid) {
    return (fernos_error_t)trigger_syscall(SCID_SIGNAL, (uint32_t)pid, (uint32_t)sid, 0, 0);
}

sig_vector_t sc_signal_allow(sig_vector_t sv) {
    return  (sig_vector_t)trigger_syscall(SCID_SIGNAL_ALLOW, (uint32_t)sv, 0, 0, 0);
}

fernos_error_t sc_signal_wait(sig_vector_t sv, sig_id_t *sid) {
    return (fernos_error_t)trigger_syscall(SCID_SIGNAL_WAIT, (uint32_t)sv, (uint32_t)sid, 0, 0);
}

void sc_signal_clear(sig_vector_t sv) {
    trigger_syscall(SCID_SIGNAL_CLEAR, (uint32_t)sv, 0, 0, 0);
}

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

void sc_term_put_fmt_s(const char *fmt, ...) {
    char buf[256];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    sc_term_put_s(buf);
}

fernos_error_t sc_handle_cmd(handle_t h, handle_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return (fernos_error_t)trigger_syscall(handle_cmd_scid(h, cmd_id), arg0, arg1, arg2, arg3);
}

void sc_handle_close(handle_t h) {
    sc_handle_cmd(h, HCID_CLOSE, 0, 0, 0, 0);
}

fernos_error_t sc_handle_write(handle_t h, const void *src, size_t len, size_t *written) {
    return sc_handle_cmd(h, HCID_WRITE, (uint32_t)src, len, (uint32_t)written, 0);
}

fernos_error_t sc_handle_read(handle_t h, void *dest, size_t len, size_t *readden) {
    return sc_handle_cmd(h, HCID_READ, (uint32_t)dest, len, (uint32_t)readden, 0);
}

fernos_error_t sc_handle_wait(handle_t h) {
    return sc_handle_cmd(h, HCID_WAIT, 0, 0, 0, 0);
}

fernos_error_t sc_plg_cmd(plugin_id_t plg_id, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return (fernos_error_t)trigger_syscall(plugin_cmd_scid(plg_id, cmd_id), arg0, arg1, arg2, arg3);
}

fernos_error_t sc_handle_write_full(handle_t h, const void *src, size_t len) {
    fernos_error_t err;

    size_t written = 0;
    while (written < len) {
        size_t tmp_written;
        err = sc_handle_write(h, (const uint8_t *)src + written, len - written, &tmp_written);
        if (err != FOS_SUCCESS) {
            return err;
        }

        written += tmp_written;
    }

    return FOS_SUCCESS;
}

fernos_error_t sc_handle_read_full(handle_t h, void *dest, size_t len) {
    fernos_error_t err;

    size_t readden = 0;
    while (readden < len) {
        size_t tmp_readden;
        err = sc_handle_read(h, (uint8_t *)dest + readden, len - readden, &tmp_readden);

        if (err == FOS_EMPTY) { // block!
            err = sc_handle_wait(h);
            if (err != FOS_SUCCESS) {
                return err;
            }
        } else if (err == FOS_SUCCESS) {
            readden += tmp_readden; // Successful read.
        } else {
            return err; // Unexpected error.
        }
    }

    return FOS_SUCCESS;
}
