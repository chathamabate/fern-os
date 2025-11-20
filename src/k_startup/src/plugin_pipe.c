
#include "k_startup/plugin_pipe.h"
#include "k_startup/page_helpers.h"

/*
 * Pipe handle state stuff.
 */

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_pipe_handle_state(handle_state_t *hs);

static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written); 
static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden);
static fernos_error_t pipe_hs_wait(handle_state_t *hs);
static fernos_error_t pipe_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3);

const handle_state_impl_t KB_HS_IMPL = {
    .copy_handle_state = copy_pipe_handle_state,
    .delete_handle_state = delete_pipe_handle_state,
    .hs_write = pipe_hs_write,
    .hs_read = pipe_hs_read,
    .hs_wait = pipe_hs_wait,
    .hs_cmd = pipe_hs_cmd
};

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t delete_pipe_handle_state(handle_state_t *hs) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_wait(handle_state_t *hs) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_PIPE_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = pipe_plg_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reset_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_pipe_plugin(kernel_state_t *ks);

static fernos_error_t pipe_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return FOS_E_NOT_IMPLEMENTED;
}
