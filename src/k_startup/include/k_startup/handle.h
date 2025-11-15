
#pragma once

#include "s_util/err.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/process.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"

/*
 * A "Handle" will be the new generic way of reading/writing from 
 * userspace to the kernel.
 *
 * This will slightly mimic linux character devices. The intention
 * being that one interface can be used for speaking with a variety
 * of different things! (Files, hardware devices, pipes, etc...)
 */

typedef struct _handle_state_impl_t {
    // REQUIRED!
    fernos_error_t (*copy_handle_state)(handle_state_t *hs, process_t *proc, handle_state_t **out);
    fernos_error_t (*delete_handle_state)(handle_state_t *hs);

    // OPTIONAL!
    fernos_error_t (*hs_write)(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
    fernos_error_t (*hs_read)(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
    fernos_error_t (*hs_wait)(handle_state_t *hs);
    fernos_error_t (*hs_cmd)(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
} handle_state_impl_t;

struct _handle_state_t {
    const handle_state_impl_t * const impl;
    
    /**
     *  The kernel state this handle state belongs to.
     */
    kernel_state_t * const ks;

    /**
     * The process this handle state belongs to.
     */
    process_t * const proc;

    /**
     * The handle which maps to this state in `proc`.
     */
    const handle_t handle;

    /**
     * Whether or not this is a character display handle.
     */
    const bool is_cd;
};

static inline void init_base_handle(handle_state_t *hs, const handle_state_impl_t *impl, kernel_state_t *ks,
        process_t *proc, handle_t handle, bool is_cd) {
    *(const handle_state_impl_t **)&(hs->impl) = impl;
    *(kernel_state_t **)&(hs->ks) = ks;
    *(process_t **)&(hs->proc) = proc;
    *(handle_t *)&(hs->handle) = handle;
    *(bool *)&(hs->is_cd) = is_cd;
}

/**
 * Create a copy of this handle state however you see fit.
 * This will likely only be done when a process forks.
 *
 * `proc` is the process of the copied handle state will belong to.
 * The created handle state should have the EXACT same handle as `hs`. 
 * NOTE: This call SHOULD NOT modify `proc`'s handle table, that will be done automatically
 * after this function is called.
 *
 * Just create a new valid handle state and write it to `*out`.
 *
 * If this call returns FOS_E_ABORT_SYSTEM, the system will lock up.
 * If this call return some other non-success error code, the fork will fail.
 */
static inline fernos_error_t copy_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    return hs->impl->copy_handle_state(hs, proc, out);
}

/**
 * Delete this handle state!
 *
 * NOTE: In the case of a process being reaped, this is called BEFORE all other process resources
 * are deleted. i.e. the `proc` field in `hs` will still be valid when this call executes.
 * (This will be called before the plugin on reap handlers)
 *
 * This can do other things, like flush buffers as needed.
 * Like with copy, this should NEVER modify the parent process's handle table, that will be
 * done after this function is called.
 *
 * If this handle belongs to some plugin, how it interacts with said plugin on deletion is
 * up to you! (i.e. just be careful)
 *
 * If a destructor ever returns any error at all, The system will lock up.
 */
static inline fernos_error_t delete_handle_state(handle_state_t *hs) {
    if (hs) {
        return hs->impl->delete_handle_state(hs); 
    }
    return FOS_E_SUCCESS;
}

/*
 * NOTE VERY IMPORTANT: The below endpoints should only ever be triggered during some sort of 
 * system call. THESE ALL ASSUME A CURRENT THREAD!
 *
 * The kernel will always use these in this way. However, plugins can kinda do whatever they like.
 * So, be careful! If you want your plugin to invoke some sort of handle call, make sure
 * there is a current thread which expects the results!
 *
 * If any of these return an error here in kernel space, the system shoud lock up!
 * Normal errors are expected to be returned to userspace.
 */

/**
 * This will map to the close handle system call. It simply calls the destructor and returns
 * the given handle ID to the handle table.
 *
 * Close is a void function from the user's perspective.
 */
KS_SYSCALL static inline fernos_error_t hs_close(handle_state_t *hs) {
    fernos_error_t err;
    handle_t h = hs->handle;

    // Failure to delete a handle is seen as a catastrophic error.
    err = delete_handle_state(hs);
    if (err != FOS_E_SUCCESS) {
        return FOS_E_ABORT_SYSTEM;
    }

    idtb_push_id(hs->proc->handle_table, h);

    return FOS_E_SUCCESS;
}

/**
 * Write data to the handle.
 *
 * `u_src` and `u_written` are both userspace pointers.
 *
 * `len` is the amount of data to attempt to write from `u_src`.
 * `u_written` is optional. On success, if `u_written` is provided, the number of bytes successfully
 * written should be stored in `*u_written`.
 */
KS_SYSCALL static inline fernos_error_t hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    if (hs->impl->hs_write) {
        return hs->impl->hs_write(hs, u_src, len, u_written);
    }
    DUAL_RET((thread_t *)(hs->ks->schedule.head), FOS_E_NOT_IMPLEMENTED, FOS_E_SUCCESS);
}

/**
 * Read data from the handle.
 *
 * `u_dst` and `u_readden` are both userspace pointers.
 *
 * `len` is the amount of data to attempt to read to `u_dst`.
 * `u_readden` is optional. On success, if `u_readden` is provided, the number of bytes successfully
 * read should be stored in `*u_readden`.
 *
 * If ANY DATA at all is read, FOS_E_SUCCESS is returned.
 * If there is no data to read at this time, FOS_E_EMPTY is returned!
 *
 * This call should NEVER BLOCK. 
 */
KS_SYSCALL static inline fernos_error_t hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden) {
    if (hs->impl->hs_read) {
        return hs->impl->hs_read(hs, u_dst, len, u_readden);
    }
    DUAL_RET((thread_t *)(hs->ks->schedule.head), FOS_E_NOT_IMPLEMENTED, FOS_E_SUCCESS);
}

/**
 * Block until there is data to read from a given handle.
 *
 * FOS_E_SUCCESS means there is data to read!
 * FOS_E_EMPTY means there will NEVER be anymore data to read from this handle!
 */
KS_SYSCALL static inline fernos_error_t hs_wait(handle_state_t *hs) {
    if (hs->impl->hs_wait) {
        return hs->impl->hs_wait(hs);
    }
    DUAL_RET((thread_t *)(hs->ks->schedule.head), FOS_E_NOT_IMPLEMENTED, FOS_E_SUCCESS);
}

/**
 * Perform a custom command on the handle.
 */
KS_SYSCALL static inline fernos_error_t hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (hs->impl->hs_cmd) {
        return hs->impl->hs_cmd(hs, cmd, arg0, arg1, arg2, arg3);
    }
    DUAL_RET((thread_t *)(hs->ks->schedule.head), FOS_E_NOT_IMPLEMENTED, FOS_E_SUCCESS);
}
