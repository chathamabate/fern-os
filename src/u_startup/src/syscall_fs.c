

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

fernos_error_t sc_fs_seek(handle_t h, size_t pos) {
    return sc_handle_cmd(h, PLG_FS_HCID_SEEK, pos, 0, 0, 0);
}

fernos_error_t sc_fs_flush(handle_t h) {
    return sc_handle_cmd(h, PLG_FS_PCID_FLUSH, 0, 0, 0, 0);
}
