

#include "u_startup/syscall_fs.h"
#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_util/str.h"

fernos_error_t sc_fs_set_wd(const char *path) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_SET_WD, (uint32_t)path, str_len(path), 0, 0);
}

fernos_error_t sc_fs_touch(const char *path) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_TOUCH, (uint32_t)path, str_len(path), 0, 0);
}

fernos_error_t sc_fs_mkdir(const char *path) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_MKDIR, (uint32_t)path, str_len(path), 0, 0);
}

fernos_error_t sc_fs_remove(const char *path) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_REMOVE, (uint32_t)path, str_len(path), 0, 0);
}

fernos_error_t sc_fs_get_info(const char *path, fs_node_info_t *info) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_GET_INFO, (uint32_t)path, str_len(path), (uint32_t)info, 0);
}

fernos_error_t sc_fs_get_child_name(const char *path, size_t index, char *child_name) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_GET_CHILD_NAME, (uint32_t)path, str_len(path), index, (uint32_t)child_name);
}

fernos_error_t sc_fs_flush_all(void) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_FLUSH, 0, 0, 0, 0);
}

fernos_error_t sc_fs_open(const char *path, handle_t *fh) {
    return sc_plg_cmd(PLG_FILE_SYS_ID, PLG_FS_PCID_OPEN, (uint32_t)path, str_len(path), (uint32_t)fh, 0);
}

/*
void sc_fs_close(file_handle_t fh) {
    trigger_syscall(SCID_FS_CLOSE, (uint32_t)fh, 0, 0, 0);
}

fernos_error_t sc_fs_seek(file_handle_t fh, size_t pos) {
    return trigger_syscall(SCID_FS_SEEK, (uint32_t)fh, pos, 0, 0);
}

fernos_error_t sc_fs_write(file_handle_t fh, const void *src, size_t len, size_t *written) {
    return trigger_syscall(SCID_FS_WRITE, (uint32_t)fh, (uint32_t)src, len, (uint32_t)written);
}

fernos_error_t sc_fs_write_full(file_handle_t fh, const void *src, size_t len) {
    fernos_error_t err;

    size_t written = 0;
    while (written < len) {
        size_t tmp_written;
        err = sc_fs_write(fh, (const uint8_t *)src + written, len - written, &tmp_written);
        if (err != FOS_SUCCESS) {
            return err;
        }

        written += tmp_written;
    }

    return FOS_SUCCESS;
}

fernos_error_t sc_fs_read(file_handle_t fh, void *dst, size_t len, size_t *readden) {
    return trigger_syscall(SCID_FS_READ, (uint32_t)fh, (uint32_t)dst, len, (uint32_t)readden);
}

fernos_error_t sc_fs_read_full(file_handle_t fh, void *dst, size_t len) {
    fernos_error_t err;

    size_t readden = 0;
    while (readden < len) {
        size_t tmp_readden;
        err = sc_fs_read(fh, (uint8_t *)dst + readden, len - readden, &tmp_readden);
        if (err != FOS_SUCCESS) {
            return err;
        }

        readden += tmp_readden;
    }

    return FOS_SUCCESS;
}

fernos_error_t sc_fs_flush(file_handle_t fh) {
    return trigger_syscall(SCID_FS_FLUSH, (uint32_t)fh, 0, 0, 0);
}
*/
