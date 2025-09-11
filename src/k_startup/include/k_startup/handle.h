
#pragma once

#include "s_util/err.h"
#include "k_startup/fwd_defs.h"
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
    void (*delete_handle_state)(handle_state_t *hs);

    // OPTIONAL!
    fernos_error_t (*hs_write)(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);
    fernos_error_t (*hs_read)(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden);
    fernos_error_t (*hs_cmd)(handle_state_t *hs, handle_cmd_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
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
};

static inline void init_base_handle(handle_state_t *hs, const handle_state_impl_t *impl, kernel_state_t *ks,
        process_t *proc, handle_t handle) {
    *(const handle_state_impl_t **)&(hs->impl) = impl;
    *(kernel_state_t **)&(hs->ks) = ks;
    *(process_t **)&(hs->proc) = proc;
    *(handle_t *)&(hs->handle) = handle;
}

/**
 * Create a copy of this handle state however you see fit.
 * This will likely only be done when a process forks.
 *
 * `proc` is the parent process of the copied handle state.
 * The created handle state should have the EXACT same handle as `hs`. 
 * NOTE: This call SHOULD NOT modify `proc`'s handle table, that will be done automatically
 * after this function is called.
 *
 * Just create a new valid handle state and write it to `*out`.
 */
static inline fernos_error_t copy_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    return hs->impl->copy_handle_state(hs, proc, out);
}

/**
 * Delete this handle state!
 *
 * This can do other things, like flush buffers as needed.
 * Like with copy, this should NEVER modify the parent process's handle table, that will be
 * done after this function is called.
 *
 * If this handle belongs to some plugin, how it interacts with said plugin on deletion is
 * up to you! (i.e. just be careful)
 */
static inline void delete_handle_state(handle_state_t *hs) {
    if (hs) {
        hs->impl->delete_handle_state(hs); 
    }
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
 * Write data to the handle.
 *
 * `u_src` and `u_written` are both userspace pointers.
 *
 * `len` is the amount of data to attempt to write from `u_src`.
 * `u_written` is optional. On success, if `u_written` is provided, the number of bytes successfully
 * written should be stored in `*u_written`.
 */
static inline fernos_error_t hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    if (hs->impl->hs_write) {
        return hs->impl->hs_write(hs, u_src, len, u_written);
    }
    DUAL_RET(hs->ks->curr_thread, FOS_NOT_IMPLEMENTED, FOS_SUCCESS);
}

/**
 * Read data from the handle.
 *
 * `u_dst` and `u_readden` are both userspace pointers.
 *
 * `len` is the amount of data to attempt to read to `u_dst`.
 * `u_readden` is optional. On success, if `u_readden` is provided, the number of bytes successfully
 * read should be stored in `*u_readden`.
 */
static inline fernos_error_t hs_read(handle_state_t *hs, void *u_dst, size_t len, size_t *u_readden) {
    if (hs->impl->hs_read) {
        return hs->impl->hs_read(hs, u_dst, len, u_readden);
    }
    DUAL_RET(hs->ks->curr_thread, FOS_NOT_IMPLEMENTED, FOS_SUCCESS);
}

/**
 * Perform a custom command on the handle.
 */
static inline fernos_error_t hs_cmd(handle_state_t *hs, handle_cmd_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (hs->impl->hs_cmd) {
        return hs->impl->hs_cmd(hs, cmd, arg0, arg1, arg2, arg3);
    }
    DUAL_RET(hs->ks->curr_thread, FOS_NOT_IMPLEMENTED, FOS_SUCCESS);
}


