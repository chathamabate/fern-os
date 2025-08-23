
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
     
        path_str_offset += path_processed;

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
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_fs_node_key_t nk = (fat32_fs_node_key_t)key;

    al_free(fat32_fs->al, (void *)nk);
}

static bool fat32_fs_key_equator(const void *k0, const void *k1) {
    fat32_fs_node_key_t nk0 = *(const fat32_fs_node_key_t *)k0;
    fat32_fs_node_key_t nk1 = *(const fat32_fs_node_key_t *)k1;

    // Given both these keys were created using the valid fat32_fs endpoints, 
    // we can safely just compare each node's starting cluster index.
    return (nk0->starting_slot_ind == nk1->starting_slot_ind);
}

static equator_ft fat32_fs_get_key_equator(file_sys_t *fs) {
    (void)fs;

    return fat32_fs_key_equator;
}

static uint32_t fat32_fs_key_hasher(const void *k) {
    fat32_fs_node_key_t nk = *(const fat32_fs_node_key_t *)k;

    // Some random hash function.
    return (((nk->starting_slot_ind * 31) + 11) * 7) + 3;
}

static hasher_ft fat32_fs_get_key_hasher(file_sys_t *fs) {
    (void)fs;

    return fat32_fs_key_hasher;
}

static fernos_error_t fat32_fs_get_node_info(file_sys_t *fs, fs_node_key_t key, fs_node_info_t *info) {
    if (!key || !info) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_device_t *dev = fat32_fs->dev;
    fat32_fs_node_key_t nk = (fat32_fs_node_key_t)key;

    if (nk->is_dir) {
        // Remember this implementation doesn't give directory edited/creation times.
        info->creation_dt = (fernos_datetime_t) { 0 };
        info->last_edited_dt = (fernos_datetime_t) { 0 };

        info->is_dir = true;

        // Ok, now we need the "length" of the directory, i.e. the number of sequences 
        // containing LFN entries. 

        uint32_t dir_len = 0;
        uint32_t entry_iter = 0;

        while (true) {
            uint32_t seq_start;
            err = fat32_next_dir_seq(dev, nk->starting_slot_ind, entry_iter, &seq_start);
            if (err == FOS_EMPTY) {
                break;
            }

            if (err != FOS_SUCCESS) {
                return err;
            }

            uint32_t sfn_entry_offset;
            err = fat32_get_dir_seq_sfn(dev, nk->starting_slot_ind, seq_start, &sfn_entry_offset);
            if (err != FOS_SUCCESS) {
                return err;
            }

            // We only recognize sequences with LFN entries.
            if (seq_start < sfn_entry_offset) {
                dir_len++;
            }

            entry_iter = sfn_entry_offset + 1;
        }

        info->len = dir_len;
    } else {
        fat32_short_fn_dir_entry_t sfn_entry;

        err = fat32_read_dir_entry(dev, nk->parent_slot_ind, nk->sfn_entry_offset, 
                (fat32_dir_entry_t *)&sfn_entry);
        if (err != FOS_SUCCESS) {
            return err;
        }

        fat32_datetime_to_fos_datetime(sfn_entry.creation_date, sfn_entry.creation_time, &(info->creation_dt));
        fat32_datetime_to_fos_datetime(sfn_entry.last_write_date, sfn_entry.last_write_time, &(info->last_edited_dt));

        info->is_dir = false;

        info->len = sfn_entry.files_size;
    }

    return FOS_SUCCESS;
}

static fernos_error_t fat32_fs_touch(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key) {
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_device_t *dev = fat32_fs->dev;

    if (!name || !parent_dir || !key) {
        return FOS_BAD_ARGS;
    }

    fat32_fs_node_key_t pd = (fat32_fs_node_key_t)parent_dir;

    if (!(pd->is_dir)) {
        return FOS_STATE_MISMATCH;
    }

    // Ok, now we need to check the filename given.

    if (!is_valid_filename(name)) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;
    uint32_t seq_start;

    err = fat32_find_lfn_c8(dev, pd->starting_slot_ind, name, &seq_start);
    if (err == FOS_SUCCESS) {
        return FOS_IN_USE;
    }

    if (err != FOS_EMPTY) {
        return FOS_UNKNWON_ERROR;
    }

    // Now we have a valid filenmae which does not appear in the given directory.
    // We must write it into the parent directory!

    // Get the current datetime.
    fernos_datetime_t now;
    fat32_fs->now(&now);

    fat32_date_t fat32_d;
    fat32_time_t fat32_t;

    fos_datetime_to_fat32_datetime(now, &fat32_d, &fat32_t);

    fat32_short_fn_dir_entry_t sfn_entry = {
        .creation_date = fat32_d,
        .creation_time = fat32_t,
        .creation_time_hundredths = 0,
        .files_size = 0,
        .last_write_date = fat32_d,
        .last_write_time = fat32_t,
        .last_access_date = 0, // we aren't going to use last access date in this impl.
        
        // We need to fill these in before doing anything else.
        .short_fn = { 0 },
        .extenstion = {' ', ' ', ' '},

        .first_cluster_high = 0,
        .first_cluster_low = 0,
    };

    while (true) {
        // Let's generate a random SFN which is yet to be used in the given directory.

        // I think we are going to want to factor out a lot of this tbh...
        // This will mostly be the same between directory and file creation.
    }



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



