
#pragma once

#include "s_data/id_table.h"
#include "s_mem/allocator.h"

/*
 * This header includes type definitions which are significant in both user and kernel space.
 */

/**
 * The globally unique ID of a process. (Can be 0)
 */
typedef id_t proc_id_t;

/**
 * A signal ID is within the range [0,31]
 */
typedef id_t sig_id_t;

/**
 * This is the only *special* signal type as of now.
 *
 * When a process exits, this signal is sent to the parent!
 */
#define FSIG_CHLD (0x0U)

/**
 * This is a bit vector which can be used to represent pending signals, or
 * signals which are meant to be allowed.
 */
typedef uint32_t sig_vector_t;

static inline sig_vector_t empty_sig_vector(void) {
    return 0;
}

static inline sig_vector_t full_sig_vector(void) {
    return ~(sig_vector_t)0;
}

static inline void sv_add_signal(sig_vector_t *sv, sig_id_t sid) {
    if (sid < 32) {
        *sv |= (1 << sid);
    }
}

static inline void sv_remove_signal(sig_vector_t *sv, sig_id_t sid) {
    if (sid < 32) {
        *sv &= ~(1 << sid);
    }
}

/**
 * The process unique ID of a thread. (Can be 0)
 */
typedef id_t thread_id_t;

/**
 * Created threads must be entered via a function conforming to this siganture!
 */
typedef void *(*thread_entry_t)(void *arg);

/**
 * A vector for specifying which 
 */
typedef uint32_t join_vector_t;

static inline join_vector_t empty_join_vector(void) {
    return 0;
}

/**
 * A vector which accepts any thread
 */
static inline join_vector_t full_join_vector(void) {
    return ~(join_vector_t)0;
}

static inline void jv_add_tid(join_vector_t *jv, thread_id_t tid) {
    if (tid < 32) {
        *jv |= (1 << tid);
    }
}

static inline void jv_remove_tid(join_vector_t *jv, thread_id_t tid) {
    if (tid < 32) {
        *jv &= ~(1 << tid);
    }
}

/**
 * When a thread is joined, these values should be copied back to user space.
 */
typedef struct _thread_join_ret_t {
    thread_id_t joined;
    void *retval;
} thread_join_ret_t;

/**
 * A futex is just a 32-bit integer.
 */
typedef int32_t futex_t;

/**
 * Exit statuses of a process.
 */
typedef uint32_t proc_exit_status_t;

#define PROC_ES_SUCCESS (0x0U)
#define PROC_ES_UNSET   (0x1U)

#define PROC_ES_FAILURE (0x2U)
#define PROC_ES_GPF (0x3U) // Exit due to a general protection fault.
#define PROC_ES_PF  (0x4U) // Exit due to a page fault.
#define PROC_ES_SIGNAL  (0x5U) // An unallowed signal was received.

/*
 * Syscall IDs.
 */

typedef uint32_t syscall_id_t;

/**
 * The final two bits of a syscall are the "attrs".
 * If "attrs" = 0b00, Vanilla Syscall
 * If "attrs" = 0b01, Handle Syscall
 * If "attrs" = 0b10, Plugin Syscall 
 */
#define SYSCALL_ATTRS_MASK (3UL << 30) 

/*
 * Vanilla Syscalls
 */

#define VANILLA_CMD_PREFIX (0x0UL)

static inline bool scid_is_vanilla(syscall_id_t scid) {
    return (scid & SYSCALL_ATTRS_MASK) == VANILLA_CMD_PREFIX;
}

/* Process Syscalls */
#define SCID_PROC_FORK (0x80U)
#define SCID_PROC_EXIT (0x81U)
#define SCID_PROC_REAP (0x82U)
#define SCID_PROC_EXEC (0x83U)

/* Signal Syscalls (process adjacent) */
#define SCID_SIGNAL       (0x90U)
#define SCID_SIGNAL_ALLOW (0x91U)
#define SCID_SIGNAL_WAIT  (0x92U)
#define SCID_SIGNAL_CLEAR (0x93U)

/* Process Memory Management */
#define SCID_MEM_REQUEST  (0xA0U)
#define SCID_MEM_RETURN   (0xA1U)

/* Thread Syscalls */
#define SCID_THREAD_EXIT  (0x100U)
#define SCID_THREAD_SLEEP (0x101U)
#define SCID_THREAD_SPAWN (0x102U)
#define SCID_THREAD_JOIN  (0x103U)

/* Default IO Syscalls (See Handle Syscalls) */
#define SCID_SET_IN_HANDLE  (0x300U)
#define SCID_GET_IN_HANDLE  (0x301U)
#define SCID_IN_READ        (0x302U)
#define SCID_IN_WAIT        (0x303U) // wait_read_ready
#define SCID_SET_OUT_HANDLE (0x304U)
#define SCID_GET_OUT_HANDLE (0x305U)
#define SCID_OUT_WRITE      (0x306U) 
#define SCID_OUT_WAIT       (0x307U) // wait_write_ready

/*
 * Handle Syscalls
 */

/*
 * The handle system call id will take the following form:
 *
 * [0:15]  handle_cmd_id
 * [16:23] handle
 * [24:31] 0x40
 */

/**
 * NOTE: a handle will only ever require 8 bits, but because handles will be placed
 * in ID Tables, the "NULL" handle will be 256 (which does not fit in 8 bits)
 *
 * So, the handle type will actually be 32 bits to make iteration/operations 
 * less error prone.
 */
typedef id_t handle_t;
typedef uint16_t handle_cmd_id_t;

#define HANDLE_CMD_PREFIX (1UL << 30)

static inline bool scid_is_handle_cmd(syscall_id_t scid) {
    return (scid & SYSCALL_ATTRS_MASK) == HANDLE_CMD_PREFIX;
}

#define HCID_CLOSE            (0x0U)
#define HCID_WAIT_WRITE_READY (0x1U)
#define HCID_WRITE            (0x2U)
#define HCID_WAIT_READ_READY  (0x3U)
#define HCID_READ             (0x4U)
#define HCID_IS_CD            (0x5U)

#define NUM_DEFAULT_HCIDS (HCID_IS_CD + 1)

// All other handle commands are custom/implementation specific!

static inline uint32_t handle_cmd_scid(handle_t h, handle_cmd_id_t cmd) {
    return HANDLE_CMD_PREFIX | ((h & 0xFF) << 16) | cmd;
}

static inline void handle_scid_extract(syscall_id_t scid, handle_t *h, handle_cmd_id_t *hcid) {
    *h = (scid >> 16) & 0xFF;
    *hcid = (handle_cmd_id_t)scid;
}

/*
 * ***** Character Display Handle *****
 *
 * A handle which references a character display will support the following command IDs.
 */

#define CD_HCID_GET_DIMS (NUM_DEFAULT_HCIDS + 0)


/*
 * Plugin Command syntax
 *
 * [0:15]  plugin_cmd_id
 * [16:23] plugin_id
 * [24:31] 0x80
 */

/**
 * The globally unique ID of a plugin.
 *
 * This is 32 bits for the same reason `handle_t` is 32 bits.
 */
typedef uint32_t plugin_id_t;

/*
 * Note the Plugin command 0x1 is different from the Plugin kernel command 0x1.
 */

/**
 * The Plugin unique ID of a kernel command.
 */
typedef uint32_t plugin_kernel_cmd_id_t;

/**
 * The Plugin unique ID of a command.
 */
typedef uint16_t plugin_cmd_id_t;

#define PLUGIN_CMD_PREFIX (2UL << 30)

static inline bool scid_is_plugin_cmd(syscall_id_t scid) {
    return (scid & SYSCALL_ATTRS_MASK) == PLUGIN_CMD_PREFIX;
}

/*
 * NOTE: Unlike Handles, Plugins have NO default command IDs, plugins have no special builtin
 * endpoints.
 */

static inline uint32_t plugin_cmd_scid(plugin_id_t plg_id, plugin_cmd_id_t cmd_id) {
    return PLUGIN_CMD_PREFIX | ((plg_id & 0xFF) << 16) | cmd_id;
}

static inline void plugin_scid_extract(uint32_t plg_scid, plugin_id_t *plg_id, plugin_cmd_id_t *cmd_id) {
    *plg_id = (plg_scid >> 16) & 0xFF;
    *cmd_id = (uint16_t)plg_scid;
}

/*
 * ***** Futex Plugin *****
 */

#define PLG_FUTEX_ID (0U)

#define PLG_FUT_PCID_REGISTER   (0x0U)
#define PLG_FUT_PCID_DEREGISTER (0x1U)
#define PLG_FUT_PCID_WAIT       (0x2U)
#define PLG_FUT_PCID_WAKE       (0x3U)

#define PLG_FUTEX_NUM_CMDS (PLG_FUT_PCID_WAKE + 1)

/*
 * ***** File System Plugin *****
 */

#define PLG_FILE_SYS_ID (1U)

/*
 * File system plugin commands.
 */

#define PLG_FS_PCID_SET_WD         (0U)
#define PLG_FS_PCID_TOUCH          (1U)
#define PLG_FS_PCID_MKDIR          (2U)
#define PLG_FS_PCID_REMOVE         (3U)
#define PLG_FS_PCID_GET_INFO       (4U)
#define PLG_FS_PCID_GET_CHILD_NAME (5U)
#define PLG_FS_PCID_FLUSH          (6U)
#define PLG_FS_PCID_OPEN           (7U)

#define PLG_FILE_SYS_NUM_CMDS      (PLG_FS_PCID_OPEN + 1)

/*
 * File system plugin handle commands
 */

#define PLG_FS_HCID_SEEK           (NUM_DEFAULT_HCIDS + 0U)
#define PLG_FS_HCID_FLUSH          (NUM_DEFAULT_HCIDS + 1U)

/*
 * ***** Keyboard Plugin ******
 */

#define PLG_KEYBOARD_ID (2U)

/*
 * Keyboard plugin KERNEL commands.
 */

#define PLG_KB_KCID_KEY_EVENT     (0U)

/*
 * Keyboard plugin commands.
 */

#define PLG_KB_PCID_OPEN          (0U)

/*
 * Keyboard plugin handle commands
 */

#define PLG_KB_HCID_SKIP_FWD      (NUM_DEFAULT_HCIDS + 0U)

/*
 * ***** VGA Character Display Plugin *****
 */

#define PLG_VGA_CD_ID         (3U)

/*
 * VGA Character Display Plugin commands.
 */

#define PLG_VGA_CD_PCID_OPEN      (0U)

/*
 * ***** Pipe Plugin *****
 */

#define PLG_PIPE_ID         (4U)

/*
 * Pipe Plugin Commands.
 */

#define PLG_PIPE_PCID_OPEN      (0U)
