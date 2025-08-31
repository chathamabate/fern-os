
#pragma once
#include "k_startup/state_fs.h"

fernos_error_t ks_fs_set_wd(kernel_state_t *ks, const char *u_path, size_t u_path_len);
fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len, file_handle_t *u_fh);
fernos_error_t ks_fs_mkdir(kernel_state_t *ks, const char *u_path, size_t u_path_len);
fernos_error_t ks_fs_get_info(kernel_state_t *ks, const char *u_path, size_t u_path_len, fs_node_info_t *u_info);
fernos_error_t ks_fs_get_child_name(kernel_state_t *ks, const char *u_path, size_t u_path_len, size_t index, char *u_child_name);
fernos_error_t ks_fs_open(kernel_state_t *ks, char *u_path, size_t u_path_len, file_handle_t *u_fh);
void ks_fs_close(kernel_state_t *ks, file_handle_t fh);
fernos_error_t ks_fs_seek(kernel_state_t *ks, file_handle_t fh, size_t pos);
fernos_error_t ks_fs_write(kernel_state_t *ks, file_handle_t fh, const void *u_src, size_t len, size_t *u_written);
fernos_error_t ks_fs_read(kernel_state_t *ks, file_handle_t fh, void *u_dst, size_t len, size_t *u_readden);
