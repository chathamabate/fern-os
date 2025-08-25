
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

    // We ultimately use the given seed to create two random number generators.
    // One in the FAT32 Device, and one here. The one here will get a different seed
    // than the one in the FAT32 device.
    fat32_fs->r = rand(seed + 0xABCD0123EEEEFEFEULL);
    
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
    char fn_buf[FS_MAX_FILENAME_LEN + 1];

    while (true) {
        uint32_t path_processed = next_filename(&(path[path_str_offset]), fn_buf);

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

        // So, there are two special cases, "." and "..", in these cases we don't search for
        // an LFN.


        uint32_t seq_start;
        if (mem_cmp(fn_buf, ".", 2)) {
            // Technically in this case we could skip reading from the directory and just not
            // do anything, but whatever.
            err = fat32_find_sfn(dev, prev_ind, ".       ", "   ", &seq_start);
        } else if (mem_cmp(fn_buf, "..", 3)) {
            err = fat32_find_sfn(dev, prev_ind, "..      ", "   ", &seq_start);
        } else {
            // Otherwise normal LFN lookup.
            err = fat32_find_lfn_c8(dev, prev_ind, fn_buf, &seq_start);
        }

        // Entry couldn't be found.
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

    char lfn_buf[FAT32_MAX_LFN_LEN + 1];

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

            // We only count entries which have a valid lfn.
            err = fat32_get_dir_seq_lfn_c8(dev, nk->starting_slot_ind, sfn_entry_offset, lfn_buf);
            if (err != FOS_SUCCESS && err != FOS_EMPTY) {
                return err;
            }

            if (err == FOS_SUCCESS && is_valid_filename(lfn_buf)) {
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

/**
 * This FAT32 FS implementation doesn't really use the SFN's. Whenever a file/directory is 
 * created, it is given a parent directory unique random SFN.
 *
 * This function generates such an SFN and writes it into `sfn_entry`.
 *
 * On success, FOS_SUCCESS is returned.
 */
static fernos_error_t fat32_fs_gen_unique_sfn(fat32_file_sys_t *fat32_fs, uint32_t slot_ind, 
        fat32_short_fn_dir_entry_t *sfn_entry) {
    if (!sfn_entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    while (true) {
        // First generate the random name.
        for (uint32_t i = 0; i < sizeof(sfn_entry->short_fn); i++) {
            sfn_entry->short_fn[i] = 'A' + next_rand_u8(&(fat32_fs->r)) % 26;
        }

        // To avoid confusion, we will not have a random extension part,
        // SFN entries generated with this function will have no extension.
        mem_set(sfn_entry->extenstion, ' ', sizeof(sfn_entry->extenstion));

        // Use this for loop if I change my mind.
        /* 
        for (uint32_t i = 0; i < sizeof(sfn_entry->extenstion); i++) {
            sfn_entry->extenstion[i] = 'A' + next_rand_u8(&(fat32_fs->r)) % 26;
        }
        */

        // next see if it is used.

        uint32_t seq_start;

        err = fat32_find_sfn(fat32_fs->dev, slot_ind, sfn_entry->short_fn, sfn_entry->extenstion, &seq_start);

        if (err == FOS_EMPTY) { // Our name is unique!
            return FOS_SUCCESS;
        }

        if (err != FOS_SUCCESS) {
            return err;
        }

        // Otherwise just repeat.
    }
}

/**
 * Very simple helper Function.
 *
 * It allocates a new cluster chain. Inside the cluster chain it places two entries:
 * The self reference ".", and the parent reference "..".
 */
static fernos_error_t fat32_fs_new_subdir_chain(fat32_file_sys_t *fat32_fs, uint32_t parent_slot_ind, 
        uint32_t *slot_ind) {
    if (!slot_ind) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    uint32_t dir_slot_ind;

    err = fat32_new_dir(fat32_fs->dev, &dir_slot_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Ok now, let's allocate our two relative entries.

    fat32_short_fn_dir_entry_t sfn_entry = {
        .attrs = FT32F_ATTR_SUBDIR,
        .extenstion = {' ', ' ', ' '},
        .short_fn = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        .first_cluster_low = (uint16_t)dir_slot_ind,
        .first_cluster_high = (uint16_t)(dir_slot_ind >> 16),

        // Remember we aren't working with creation time stuff for directories.

        .files_size = 0
    };

    uint32_t entry_offset;

    // Create self reference.
    err = fat32_new_seq_c8(fat32_fs->dev, dir_slot_ind, &sfn_entry, 
            NULL, &entry_offset);

    if (err == FOS_SUCCESS) {
        // Create parent reference.
        sfn_entry.short_fn[1] = '.';
        sfn_entry.first_cluster_low = (uint16_t)parent_slot_ind;
        sfn_entry.first_cluster_high = (uint16_t)(parent_slot_ind >> 16);

        err = fat32_new_seq_c8(fat32_fs->dev, dir_slot_ind, &sfn_entry, 
                NULL, &entry_offset);
    }

    if (err != FOS_SUCCESS) {
        // In case of error, free our chain in the fat.
        fat32_free_chain(fat32_fs->dev, dir_slot_ind);

        return err;
    }

    // Otherwise, success!!!
    *slot_ind = dir_slot_ind;

    return FOS_SUCCESS;
}

/**
 * The steps for creating a subdirectory vs. a file will be pretty similar, just going to
 * combine them into a single function.
 *
 * Follows same error return rules as `fs_touch` and `fs_mkdir`.
 */
static fernos_error_t fat32_fs_new_node(fat32_file_sys_t *fat32_fs, fat32_fs_node_key_t parent_dir, const char *name, 
        bool subdir, fat32_fs_node_key_t *key) {
    fat32_device_t *dev = fat32_fs->dev;

    if (!name || !parent_dir) {
        return FOS_BAD_ARGS;
    }

    if (!(parent_dir->is_dir)) {
        return FOS_STATE_MISMATCH;
    }

    // Ok, now we need to check the filename given.

    // Is it valid?
    if (!is_valid_filename(name)) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;
    uint32_t seq_start;

    // Is it unique?
    err = fat32_find_lfn_c8(dev, parent_dir->starting_slot_ind, name, &seq_start);
    if (err == FOS_SUCCESS) {
        return FOS_IN_USE;
    }

    if (err != FOS_EMPTY) {
        return FOS_UNKNWON_ERROR;
    }

    // Now we have a valid filenmae which does not appear in the given directory.
    // 1) We must create a unique SFN.
    // 2) We must allocate a chain in the fat for the nodes data.
    // 3) We must write the chain's start into the SFN entry, then write that entry into
    // the parent directory.

    // Get the current datetime.
    fernos_datetime_t now;
    fat32_fs->now(&now);

    fat32_date_t fat32_d;
    fat32_time_t fat32_t;

    fos_datetime_to_fat32_datetime(now, &fat32_d, &fat32_t);

    fat32_short_fn_dir_entry_t sfn_entry = {
        .creation_date = subdir ? 0 : fat32_d,
        .creation_time = subdir ? 0 : fat32_t,
        .creation_time_hundredths = 0,
        .files_size = 0,
        .last_write_date = subdir ? 0 : fat32_d,
        .last_write_time = subdir ? 0 : fat32_t,
        .last_access_date = 0, // we aren't going to use last access date in this impl.
        
        .attrs = subdir ? FT32F_ATTR_SUBDIR : 0,

        // We need to fill these in before doing anything else.
        .short_fn = { 0 },
        .extenstion = { 0 },

        .first_cluster_high = 0,
        .first_cluster_low = 0,
    };

    // Now generate a unique and random SFN.
    err = fat32_fs_gen_unique_sfn(fat32_fs, parent_dir->starting_slot_ind, &sfn_entry);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    // Let's allocate a cluster chain now.

    // Next create the FAT chain.

    uint32_t slot_ind;

    if (subdir) {
        err =  fat32_fs_new_subdir_chain(fat32_fs, parent_dir->starting_slot_ind, &slot_ind);
    } else {
        err = fat32_new_chain(dev, 1, &slot_ind);
    }

    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    // We have a chain, we must write its sequence into the parent dir!

    sfn_entry.first_cluster_low = (uint16_t)slot_ind;
    sfn_entry.first_cluster_high = (uint16_t)(slot_ind >> 16);

    uint32_t entry_offset;
    err = fat32_new_seq_c8(dev, parent_dir->starting_slot_ind, &sfn_entry, name, 
            &entry_offset); 

    // I really dislike this cleanup after each step, but ultimately, it's not so bad.

    if (err != FOS_SUCCESS) {
        fat32_free_chain(dev, slot_ind);
        
        return FOS_UNKNWON_ERROR;
    }

    uint32_t sfn_entry_offset;
    err = fat32_get_dir_seq_sfn(dev, parent_dir->starting_slot_ind, entry_offset, &sfn_entry_offset);

    if (err != FOS_SUCCESS) {
        fat32_erase_seq(dev, parent_dir->starting_slot_ind, entry_offset);
        fat32_free_chain(dev, slot_ind);

        return FOS_UNKNWON_ERROR;
    }

    if (key) {
        fat32_fs_node_key_t nk = al_malloc(fat32_fs->al, sizeof(fat32_fs_node_key_val_t));
        if (!nk) {
            fat32_erase_seq(dev, parent_dir->starting_slot_ind, entry_offset);
            fat32_free_chain(dev, slot_ind);

            return FOS_NO_MEM;
        }

        if (!subdir) {
            *(uint32_t *)&(nk->parent_slot_ind) = parent_dir->starting_slot_ind;
            *(uint32_t *)&(nk->sfn_entry_offset) = sfn_entry_offset;
        }

        *(bool *)&(nk->is_dir) = subdir;
        *(uint32_t *)&(nk->starting_slot_ind) = slot_ind; 

        *key = (fs_node_key_t)nk;
    }

    return FOS_SUCCESS;
}

static fernos_error_t fat32_fs_touch(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key) {
    return fat32_fs_new_node((fat32_file_sys_t *)fs, (fat32_fs_node_key_t)parent_dir, name, 
            false, (fat32_fs_node_key_t *)key);
}

static fernos_error_t fat32_fs_mkdir(file_sys_t *fs, fs_node_key_t parent_dir, const char *name, fs_node_key_t *key) {
    return fat32_fs_new_node((fat32_file_sys_t *)fs, (fat32_fs_node_key_t)parent_dir, name, 
            true, (fat32_fs_node_key_t *)key);
}

static fernos_error_t fat32_fs_remove(file_sys_t *fs, fs_node_key_t parent_dir, const char *name) {
    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_device_t *dev = fat32_fs->dev;

    if (!parent_dir || !name) {
        return FOS_BAD_ARGS;
    }

    if (!is_valid_filename(name)) {
        return FOS_BAD_ARGS;
    }

    fat32_fs_node_key_t pd = (fat32_fs_node_key_t)parent_dir;

    if (!(pd->is_dir)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    uint32_t seq_start;
    err = fat32_find_lfn_c8(dev, pd->starting_slot_ind, name, &seq_start);
    if (err == FOS_EMPTY) {
        return FOS_INVALID_INDEX; // The file you are trying to remove doesn't exist.
    }

    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    fat32_short_fn_dir_entry_t sfn_entry;

    uint32_t sfn_offset;
    err = fat32_get_dir_seq_sfn(dev, pd->starting_slot_ind, 
            seq_start, &sfn_offset);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    err = fat32_read_dir_entry(dev, pd->starting_slot_ind, sfn_offset, (fat32_dir_entry_t *)&sfn_entry);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    uint32_t child_slot_ind = (sfn_entry.first_cluster_high << 16UL) | sfn_entry.first_cluster_low;
    
    if (sfn_entry.attrs & FT32F_ATTR_SUBDIR) {
        // Are we attempting to delete a subdirectory???
        // Iterate over all sequences within the subdirectory, if a sequence appears
        // which has lfn entries, this is NOT an empty directory.

        char lfn_buf[FAT32_MAX_LFN_LEN + 1];

        uint32_t sd_entry_iter = 0;
        while (true) {
            uint32_t sd_seq_start;
            err = fat32_next_dir_seq(dev, child_slot_ind, sd_entry_iter, &sd_seq_start);
            if (err == FOS_EMPTY) {
                break; // We made it to the end of our directory without issues!
            }

            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            uint32_t sd_seq_sfn_offset;
            err = fat32_get_dir_seq_sfn(dev, child_slot_ind, sd_seq_start, &sd_seq_sfn_offset); 
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            err = fat32_get_dir_seq_lfn_c8(dev, child_slot_ind, sd_seq_sfn_offset, lfn_buf);
            if (err != FOS_SUCCESS && err != FOS_EMPTY) {
                return FOS_UNKNWON_ERROR;
            }
            
            // We only recognize sequences with valid LFNs.
            if (err == FOS_SUCCESS && is_valid_filename(lfn_buf)) {
                return FOS_IN_USE;
            }

            sd_entry_iter = sd_seq_sfn_offset + 1;
        }
    }

    // We have made it here which means we should be able to delete our child node just fine.

    err = fat32_erase_seq(dev, pd->starting_slot_ind, seq_start);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    err = fat32_free_chain(dev, child_slot_ind);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    return FOS_SUCCESS;
}

static fernos_error_t fat32_fs_get_child_name(file_sys_t *fs, fs_node_key_t parent_dir, size_t index, char *name) {
    if (!name || !parent_dir) {
        return FOS_BAD_ARGS;
    }

    fat32_file_sys_t *fat32_fs = (fat32_file_sys_t *)fs;
    fat32_device_t *dev = fat32_fs->dev;

    fat32_fs_node_key_t pd = (fat32_fs_node_key_t)parent_dir;
    if (!(pd->is_dir)) {
        return FOS_STATE_MISMATCH;
    }

    // This is kinda slow, but I am going to stick to this strategy.
    // Remember, we can only count sequences with valid LFNs,
    // when stepping over a sequence we must check: 1) does it have an LFN? 2) Is the LFN valid?

    fernos_error_t err;
    size_t i = 0;
    char lfn_buf[FAT32_MAX_LFN_LEN + 1];
    uint32_t entry_iter = 0;

    while (true) {
        uint32_t seq_start; 

        err = fat32_next_dir_seq(dev, pd->starting_slot_ind, entry_iter, &seq_start);
        if (err == FOS_EMPTY) { // We reached the end before hitting our index :(
            return FOS_INVALID_INDEX;
        }

        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        uint32_t sfn_offset;
        err = fat32_get_dir_seq_sfn(dev, pd->starting_slot_ind, seq_start, &sfn_offset);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        err = fat32_get_dir_seq_lfn_c8(dev, pd->starting_slot_ind, sfn_offset, lfn_buf);
        if (err != FOS_SUCCESS && err != FOS_EMPTY) {
            return FOS_UNKNWON_ERROR;
        }

        if (err == FOS_SUCCESS && is_valid_filename(lfn_buf)) {
            // We got a hit! Can we exit now? or must we keep iterating?
            
            if (i == index) { 
                str_cpy(name, lfn_buf);
                return FOS_SUCCESS;
            }

            i++;
        }

        entry_iter = sfn_offset + 1;
    }
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



