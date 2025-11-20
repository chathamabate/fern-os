
#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_util/str.h"
#include "s_util/constraints.h"

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

fernos_error_t sc_proc_exec(user_app_t *ua, const void *args_block, size_t args_block_size) {
    return (fernos_error_t)trigger_syscall(SCID_PROC_EXEC, (uint32_t)ua, (uint32_t)args_block, 
            (uint32_t)args_block_size, 0);
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
    (void)trigger_syscall(SCID_SIGNAL_CLEAR, (uint32_t)sv, 0, 0, 0);
}

fernos_error_t sc_mem_request(void *s, const void *e, const void **true_e) {
    return (fernos_error_t)trigger_syscall(SCID_MEM_REQUEST, (uint32_t)s, (uint32_t)e, (uint32_t)true_e, 0);
}

void sc_mem_return(void *s, const void *e) {
    (void)trigger_syscall(SCID_MEM_RETURN, (uint32_t)s, (uint32_t)e, 0, 0);
}

const mem_manage_pair_t USER_MMP = {
    .request_mem = sc_mem_request,
    .return_mem = sc_mem_return
};

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

void sc_set_in_handle(handle_t in) {
    (void)trigger_syscall(SCID_SET_IN_HANDLE, (uint32_t)in, 0, 0, 0);
}

handle_t sc_get_in_handle(void) {
    return (handle_t)trigger_syscall(SCID_GET_IN_HANDLE, 0, 0, 0, 0);
}

fernos_error_t sc_in_read(void *dest, size_t len, size_t *readden) {
    return (fernos_error_t)trigger_syscall(SCID_IN_READ, (uint32_t)dest, (uint32_t)len, (uint32_t)readden, 0);
}

fernos_error_t sc_in_wait(void) {
    return (fernos_error_t)trigger_syscall(SCID_IN_WAIT, 0, 0, 0, 0);
}

void sc_set_out_handle(handle_t out) {
    (void)trigger_syscall(SCID_SET_OUT_HANDLE, (uint32_t)out, 0, 0, 0);
}

handle_t sc_get_out_handle(void) {
    return (handle_t)trigger_syscall(SCID_GET_OUT_HANDLE, 0, 0, 0, 0);
}

fernos_error_t sc_out_write(const void *src, size_t len, size_t *written) {
    return (fernos_error_t)trigger_syscall(SCID_OUT_WRITE, (uint32_t)src, (uint32_t)len, (uint32_t)written, 0);
}

fernos_error_t sc_out_wait(void) {
    return (fernos_error_t)trigger_syscall(SCID_OUT_WAIT, 0, 0, 0, 0);
}


fernos_error_t sc_handle_cmd(handle_t h, handle_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return (fernos_error_t)trigger_syscall(handle_cmd_scid(h, cmd_id), arg0, arg1, arg2, arg3);
}

void sc_handle_close(handle_t h) {
    sc_handle_cmd(h, HCID_CLOSE, 0, 0, 0, 0);
}

fernos_error_t sc_handle_wait_write_ready(handle_t h) {
    return sc_handle_cmd(h, HCID_WAIT_WRITE_READY, 0, 0, 0, 0);
}

fernos_error_t sc_handle_write(handle_t h, const void *src, size_t len, size_t *written) {
    return sc_handle_cmd(h, HCID_WRITE, (uint32_t)src, len, (uint32_t)written, 0);
}

fernos_error_t sc_handle_read(handle_t h, void *dest, size_t len, size_t *readden) {
    return sc_handle_cmd(h, HCID_READ, (uint32_t)dest, len, (uint32_t)readden, 0);
}

fernos_error_t sc_handle_wait_read_ready(handle_t h) {
    return sc_handle_cmd(h, HCID_WAIT_READ_READY, 0, 0, 0, 0);
}

bool sc_handle_is_cd(handle_t h) {
    return sc_handle_cmd(h, HCID_IS_CD, 0, 0, 0, 0) == FOS_E_SUCCESS;
}

fernos_error_t sc_plg_cmd(plugin_id_t plg_id, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return (fernos_error_t)trigger_syscall(plugin_cmd_scid(plg_id, cmd_id), arg0, arg1, arg2, arg3);
}

/*
 * Helpers.
 */

fernos_error_t sc_handle_write_full(handle_t h, const void *src, size_t len) {
    fernos_error_t err;

    size_t written = 0;
    while (written < len) {
        size_t tmp_written;
        err = sc_handle_write(h, (const uint8_t *)src + written, len - written, &tmp_written);
        if (err == FOS_E_SUCCESS) {
            written += tmp_written;
        } else if (err == FOS_E_EMPTY) { // this means we should wait until this handle is ready!
            err = sc_handle_wait_write_ready(h);
            if (err != FOS_E_SUCCESS) {
                return err;
            }
        } else {
            return err; // some other error.
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t sc_out_write_full(const void *src, size_t len) {
    handle_t out = sc_get_out_handle();
    if (out == FOS_MAX_HANDLES_PER_PROC) {
        return FOS_E_SUCCESS; // "everything worked fine"
    }

    return sc_handle_write_full(out, src, len);
}

fernos_error_t sc_handle_write_s(handle_t h, const char *str) {
    return sc_handle_write_full(h, str, str_len(str));
}

fernos_error_t sc_out_write_s(const char *str) {
    return sc_out_write_full(str, str_len(str));
}

fernos_error_t sc_handle_write_fmt_s(handle_t h, const char *fmt, ...) {
    char buf[SC_HANDLE_WRITE_FMT_S_BUFFER_SIZE];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    return sc_handle_write_s(h, buf);
}

fernos_error_t sc_out_write_fmt_s(const char *fmt, ...) {
    char buf[SC_HANDLE_WRITE_FMT_S_BUFFER_SIZE];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    return sc_out_write_s(buf);
}

void sc_out_write_fmt_s_lf(const char *fmt, ...) {
    char buf[SC_HANDLE_WRITE_FMT_S_BUFFER_SIZE];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    (void)sc_out_write_s(buf);
}

fernos_error_t sc_handle_read_full(handle_t h, void *dest, size_t len) {
    fernos_error_t err;

    size_t readden = 0;
    while (readden < len) {
        size_t tmp_readden;
        err = sc_handle_read(h, (uint8_t *)dest + readden, len - readden, &tmp_readden);

        if (err == FOS_E_SUCCESS) {
            readden += tmp_readden; // Successful read.
        } else if (err == FOS_E_EMPTY) { // block!
            err = sc_handle_wait_read_ready(h);
            if (err != FOS_E_SUCCESS) {
                return err;
            }
        } else {
            return err; // Unexpected error.
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t sc_in_read_full(void *dest, size_t len) {
    handle_t in = sc_get_in_handle();
    if (in == FOS_MAX_HANDLES_PER_PROC) {
        return FOS_E_EMPTY; // Nothing to ever read from the default handle.
    }

    return sc_handle_read_full(in, dest, len);
}
