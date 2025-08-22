
#include "s_block_device/fat32_file_sys.h"
#include <stdbool.h>
#include "s_util/str.h"
#include "k_bios_term/term.h"
#include "s_util/misc.h"


/*
 * FAT32 FS Work.
 */

static fernos_error_t fat32_fs_flush(file_sys_t *fs, fs_node_key_t key);
static void delete_fat32_file_sys(file_sys_t *fs);
static fernos_error_t fat32_fs_new_key(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key);
static void fat32_fs_delete_key(file_sys_t *fs, fs_node_key_t key);
static equator_ft fat32_fs_get_key_equator(file_sys_t *fs);
static hasher_ft fat32_fs_get_key_hasher(file_sys_t *fs);
static fernos_error_t fat32_fs_get_node_info(file_sys_t *fs, fs_node_key_t key, fs_node_info_t *info);
static fernos_error_t fat32_fs_touch(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key);
static fernos_error_t fat32_fs_mkdir(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key);
static fernos_error_t fat32_fs_remove(file_sys_t *fs, fs_node_key_t parent_dir, const char *name);
static fernos_error_t fat32_fs_get_child_name(file_sys_t *fs, fs_node_key_t parent_dir, size_t index, char *name);
static fernos_error_t fat32_fs_read(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, void *dest);
static fernos_error_t fat32_fs_write(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, const void *src);
static fernos_error_t fat32_fs_resize(file_sys_t *fs, fs_node_key_t file_key, size_t bytes);

static const file_sys_impl_t FAT32_FS_IMPL = {
    .fs_flush = fat32_fs_flush,
    .delete_file_sys = delete_fat32_file_sys,
    .fs_new_key = fat32_fs_new_key,
    .fs_delete_key = fat32_fs_delete_key,
    .fs_get_key_equator = fat32_fs_get_key_equator,
    .fs_get_key_hasher = fat32_fs_get_key_hasher,
    .fs_get_node_info = fat32_fs_get_node_info,
    .fs_touch = fat32_fs_touch,
    .fs_mkdir = fat32_fs_mkdir,
    .fs_remove = fat32_fs_remove,
    .fs_get_child_name = fat32_fs_get_child_name,
    .fs_read = fat32_fs_read,
    .fs_write = fat32_fs_write,
    .fs_resize = fat32_fs_resize,
};

fernos_error_t parse_new_fat32_file_sys(allocator_t *al, block_device_t *bd, uint32_t offset,
        uint64_t seed, bool dubd, file_sys_t **fs_out) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_flush(file_sys_t *fs, fs_node_key_t key) {
    return FOS_NOT_IMPLEMENTED;
}

static void delete_fat32_file_sys(file_sys_t *fs) {
}

static fernos_error_t fat32_fs_new_key(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key) {
    return FOS_NOT_IMPLEMENTED;
}

static void fat32_fs_delete_key(file_sys_t *fs, fs_node_key_t key) {
}

static equator_ft fat32_fs_get_key_equator(file_sys_t *fs) {
}

static hasher_ft fat32_fs_get_key_hasher(file_sys_t *fs) {
}

static fernos_error_t fat32_fs_get_node_info(file_sys_t *fs, fs_node_key_t key, fs_node_info_t *info) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_touch(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_mkdir(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_remove(file_sys_t *fs, fs_node_key_t parent_dir, const char *name) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_get_child_name(file_sys_t *fs, fs_node_key_t parent_dir, size_t index, char *name) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_read(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, void *dest) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_write(file_sys_t *fs, fs_node_key_t file_key, size_t offset, size_t bytes, const void *src) {
    return FOS_NOT_IMPLEMENTED;
}

static fernos_error_t fat32_fs_resize(file_sys_t *fs, fs_node_key_t file_key, size_t bytes) {
    return FOS_NOT_IMPLEMENTED;
}



