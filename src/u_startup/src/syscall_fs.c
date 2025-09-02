

#include "u_startup/syscall_fs.h"
#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"

fernos_error_t sc_fs_set_wd(const char *path, size_t path_len) {
    return trigger_syscall(SCID_FS_SET_WD, (uint32_t)path, path_len, 0, 0);
}

fernos_error_t sc_fs_touch(const char *path, size_t path_len) {
    return trigger_syscall(SCID_FS_TOUCH, (uint32_t)path, path_len, 0, 0);
}

fernos_error_t sc_fs_mkdir(const char *path, size_t path_len) {
    return trigger_syscall(SCID_FS_MKDIR, (uint32_t)path, path_len, 0, 0);
}

fernos_error_t sc_fs_remove(const char *path, size_t path_len) {
    return trigger_syscall(SCID_FS_REMOVE, (uint32_t)path, path_len, 0, 0);
}

fernos_error_t sc_fs_get_info(const char *path, size_t path_len, fs_node_info_t *info) {
    return trigger_syscall(SCID_FS_GET_INFO, (uint32_t)path, path_len, (uint32_t)info, 0);
}

fernos_error_t sc_fs_get_child_name(const char *path, size_t path_len, size_t index, char *child_name) {
    return trigger_syscall(SCID_FS_GET_CHILD_NAME, (uint32_t)path, path_len, (uint32_t)index, (uint32_t)child_name);
}

fernos_error_t sc_fs_open(const char *path, size_t path_len, file_handle_t *fh) {
    return trigger_syscall(SCID_FS_OPEN, (uint32_t)path, path_len, (uint32_t)fh, 0);
}

void sc_fs_close(file_handle_t fh) {
    trigger_syscall(SCID_FS_CLOSE, (uint32_t)fh, 0, 0, 0);
}

fernos_error_t sc_fs_seek(file_handle_t fh, size_t pos) {
    return trigger_syscall(SCID_FS_SEEK, (uint32_t)fh, pos, 0, 0);
}

fernos_error_t sc_fs_write(file_handle_t fh, const void *src, size_t len, size_t *written) {
    return trigger_syscall(SCID_FS_WRITE, (uint32_t)fh, (uint32_t)src, len, (uint32_t)written);
}

fernos_error_t sc_fs_read(file_handle_t fh, void *dst, size_t len, size_t *readden) {
    return trigger_syscall(SCID_FS_READ, (uint32_t)fh, (uint32_t)dst, len, (uint32_t)readden);
}

fernos_error_t sc_fs_flush(file_handle_t fh) {
    return trigger_syscall(SCID_FS_FLUSH, (uint32_t)fh, 0, 0, 0);
}

