
#include "s_block_device/fat32_file_sys.h"
#include <stdbool.h>
#include "s_util/str.h"
#include "s_util/misc.h"

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
        uint64_t seed, bool dubd, dt_producer_ft now, file_sys_t **fs_out) {
    if (!al || !bd || !fs_out) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    fat32_file_sys_t *fat32_fs = al_malloc(al, sizeof(fat32_file_sys_t));
    if (!fat32_fs) {
        return FOS_NO_MEM;
    }

    fat32_device_t *dev;

    err = parse_new_fat32_device(al, bd, offset, seed, dubd, &dev);
    if (err != FOS_SUCCESS) {
        al_free(al, fat32_fs);
        return err;
    }

    *(const file_sys_impl_t **)&(fat32_fs->super.impl) = &FAT32_FS_IMPL;
    *(allocator_t **)&(fat32_fs->al) = al;
    *(fat32_device_t **)&(fat32_fs->dev) = dev;
    *(dt_producer_ft *)&(fat32_fs->now) = now;
    
    *fs_out = (file_sys_t *)fat32_fs;
    return FOS_SUCCESS;
}

static fernos_error_t fat32_fs_flush(file_sys_t *fs, fs_node_key_t key) {
    (void)key;
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;

    fernos_error_t err;

    if (key == NULL) {
        // If the a full flush was requested, we will also sync the fats!
        err = fat32_sync_fats(fat32_fs->dev);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }
    
    return fat32_flush(fat32_fs->dev);
}

static void delete_fat32_file_sys(file_sys_t *fs) {
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;

    delete_fat32_device(fat32_fs->dev);
    al_free(fat32_fs->al, fat32_fs);
}

static fernos_error_t fat32_fs_new_key(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key) {
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_device_t *dev = fat32_fs->dev;
    fat32_fs_node_key_t wd = (fat32_fs_node_key_t)cwd;

    if (!path || !key) {
        return FOS_BAD_ARGS;
    }

    if (!is_valid_path(path)) {
        return FOS_BAD_ARGS; // given path must be valid.
    }

    if (wd && !(wd->is_dir)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    uint32_t dir_slot_ind;

    if (path[0] == '/') {
        dir_slot_ind = dev->root_dir_cluster;
    } else {
        // relative path.

        if (!wd) {
            // A working directory is required!
            return FOS_BAD_ARGS;
        }

        dir_slot_ind = wd->starting_slot_ind;
    }

    // Ok, now what, we traverse the given path starting at `dir_slot_ind`.

    uint32_t prev_ind = 0;  // The index into the fat of the last node we visited.
    uint32_t prev_sfn_offset = 0; // The index into the previous node which corresponds to 
                                  // the current node's SFN entry.

    // Whether or not the current node is a directory.
    bool is_dir = true;
    uint32_t curr_ind = dir_slot_ind;
    
    uint32_t path_str_offset = 0;
    char lfn_buf[FS_MAX_FILENAME_LEN + 1];

    while (true) {
        uint32_t path_processed = next_filename(&(path[path_str_offset]), lfn_buf);

        // NOTE: if this runs on the first iteration, we know our path must be "/".
        if (path_processed == 0) {
            // We have traversed our path successfully!

            fat32_fs_node_key_t nk = al_malloc(fat32_fs->al, sizeof(fat32_fs_node_key_val_t));
            if (!nk) {
                return FOS_NO_MEM;
            }

            // The reason why you cannot always just set the parent index is because of the special
            // directory entries "." and "..". If our current node is a non-directory file, we know
            // the values of `prev_ind` and `prev_sfn_offset` MUST correspond to the file's 
            // parent directory.
            if (!is_dir) {
                *(uint32_t *)&(nk->parent_slot_ind) = prev_ind;
                *(uint32_t *)&(nk->sfn_entry_offset) = prev_sfn_offset;
            }

            *(bool *)&(nk->is_dir) = is_dir;
            *(uint32_t *)&(nk->starting_slot_ind) = curr_ind; 

            *key = (fs_node_key_t)nk;

            return FOS_SUCCESS;
        }

        // Ok, otherwise there is more to traverse.
     
        if (!is_dir) { // The current node MUST be a directory for us to continue.
            return FOS_STATE_MISMATCH;
        }

        prev_ind = curr_ind;

        uint32_t seq_start;
        err = fat32_find_lfn_c8(dev, prev_ind, lfn_buf, &seq_start);
        if (err == FOS_EMPTY) {
            return FOS_EMPTY;
        }

        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        err = fat32_get_dir_seq_sfn(dev, prev_ind, seq_start, &prev_sfn_offset);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        fat32_dir_entry_t entry;
        err = fat32_read_dir_entry(dev, prev_ind, prev_sfn_offset, &entry);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        is_dir = (FT32F_ATTR_SUBDIR & entry.short_fn.attrs) == FT32F_ATTR_SUBDIR;
        curr_ind = (entry.short_fn.first_cluster_high << 16) | (entry.short_fn.first_cluster_low);
    }
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



