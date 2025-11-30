
#include "s_block_device/file_sys.h"
#include "s_util/str.h"
#include "s_util/ansi.h"

static bool fn_char_map_ready = false;
static bool fn_char_map[256];

static void init_fn_char_map(void) {
    for (uint32_t i = 0; i < 256; i++) {
        fn_char_map[i] = false;
    }

    for (char c = 'a'; c <= 'z'; c++) {
        fn_char_map[(uint8_t)c] = true;
    }

    for (char c = 'A'; c <= 'Z'; c++) {
        fn_char_map[(uint8_t)c] = true;
    }

    for (char c = '0'; c <= '9'; c++) {
        fn_char_map[(uint8_t)c] = true;
    }

    fn_char_map[(uint8_t)'-'] = true;
    fn_char_map[(uint8_t)'_'] = true;
    fn_char_map[(uint8_t)'.'] = true;

    fn_char_map_ready = true;
}

bool is_valid_filename(const char *fn) {
    if (!fn_char_map_ready) {
        init_fn_char_map();
    }

    for (size_t i = 0; ; i++) {
        char c = fn[i];

        // Reached the end of the string without issues!
        if (c == '\0') {
            // Only valid if are string is non-empty.
            return i > 0;
        }

        if (i == FS_MAX_FILENAME_LEN) {
            return false;
        }

        // Found a bad character!
        if (!fn_char_map[(uint8_t)c]) {
            return false;
        }
    }
}

bool is_valid_path(const char *path) {
    if (!fn_char_map_ready) {
        init_fn_char_map();
    }

    // Where to begin iteration from.
    size_t start = 0;

    // The start of the filenmae we are currently iterating over.
    size_t fn_start = 0;
    
    if (path[0] == '/') {
        start = 1;
        fn_start = 1;
    }

    for (size_t i = start; ; i++) {
        char c = path[i];

        if (c == '\0') {
            if (i == 0) {
                return false; // Empty paths are always invalid.
            }

            // Was the final name too long? This should work even if the path ends with a 
            // "/".
            if (i - fn_start > FS_MAX_FILENAME_LEN) {
                return false;
            }

            return true;
        }

        if (i == FS_MAX_PATH_LEN) {
            return false; // Our path is too large!
        }

        if (c == '/') {
            if (fn_start == i) {
                // We expected this character to be part of a valid file name!
                return false;
            }

            // The name we just completed parsing is too large!
            if (i - fn_start > FS_MAX_FILENAME_LEN) {
                return false;
            }

            // The next filename should start after this slash.
            fn_start = i + 1;
        } else if (!fn_char_map[(uint8_t)c]) {
            return false; // An invalid character.
        }
    }
}

size_t next_filename(const char *path, char *dest) {
    size_t fn_start = 0;

    // Skip leading slash.
    if (path[0] == '/') {
        fn_start = 1;
    }

    for (size_t i = fn_start; ; i++) {
        char c = path[i];

        if (c == '\0' || c == '/') {
            // We've reached the end of the string, or the end of a traversed filename.
            // This will still work in the case of "" or "/" paths!

            size_t len = i - fn_start;
            mem_cpy(dest, &(path[fn_start]), len);
            dest[len] = '\0';

            return i;
        }
    }
}

fernos_error_t separate_path(const char *path, char *dir, char *basename) {
    if (!path || !dir || !basename) {
        return FOS_E_BAD_ARGS;
    }

    size_t path_len; // length of the path.
    size_t last_slash = FS_MAX_PATH_LEN; // Index of the path's last "/".
    
    for (path_len = 0; path[path_len] != '\0'; path_len++) {
        if (path[path_len] == '/') {
            last_slash = path_len;
        }
    }

    // This function requires that path is non-empty! So no need to check again.

    const size_t basename_start = last_slash == FS_MAX_PATH_LEN ? 0 : last_slash + 1;

    if (basename_start == path_len) {
        return FOS_E_BAD_ARGS; // path ends in a slash :(
    }

    if (str_eq(".", path + basename_start) || str_eq("..", path + basename_start)) {
        return FOS_E_BAD_ARGS; // ends with "." or ".." :,(
    }

    if (basename_start == 0) {
        str_cpy(dir, "./");
    } else {
        mem_cpy(dir, path, last_slash + 1); 
        dir[last_slash + 1] = '\0';
    }

    str_cpy(basename, path + basename_start);

    return FOS_E_SUCCESS;
}

fernos_error_t fs_touch_path(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key) {
    fernos_error_t err;

    if (!path || !is_valid_path(path)) {
        return FOS_E_BAD_ARGS;
    }
    
    char dir[FS_MAX_PATH_LEN + 1];
    char basename[FS_MAX_FILENAME_LEN + 1];

    err = separate_path(path, dir, basename);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    if (str_eq("./", dir)) {
        // In the case of essentially just a basename is given as the path,
        // we won't bother with creating some new FS key. We'll just use the cwd.
        err = fs_touch(fs, cwd, basename, key);
    } else {
        fs_node_key_t parent_key;

        err = fs_new_key(fs, cwd, dir, &parent_key);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        err = fs_touch(fs, parent_key, basename, key);

        fs_delete_key(fs, parent_key);
    }


    return err;
}

fernos_error_t fs_mkdir_path(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key) {
    fernos_error_t err;

    if (!path || !is_valid_path(path)) {
        return FOS_E_BAD_ARGS;
    }
    
    char dir[FS_MAX_PATH_LEN + 1];
    char basename[FS_MAX_FILENAME_LEN + 1];

    err = separate_path(path, dir, basename);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    if (str_eq("./", dir)) {
        err = fs_mkdir(fs, cwd, basename, key);
    } else {
        fs_node_key_t parent_key;
        err = fs_new_key(fs, cwd, dir, &parent_key);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        err = fs_mkdir(fs, parent_key, basename, key);

        fs_delete_key(fs, parent_key);
    }

    return err;
}

fernos_error_t fs_remove_path(file_sys_t *fs, fs_node_key_t cwd, const char *path) {
    fernos_error_t err;

    if (!path || !is_valid_path(path)) {
        return FOS_E_BAD_ARGS;
    }
    
    char dir[FS_MAX_PATH_LEN + 1];
    char basename[FS_MAX_FILENAME_LEN + 1];

    err = separate_path(path, dir, basename);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    if (str_eq("./", dir)) {
        err = fs_remove(fs, cwd, basename);
    } else {
        fs_node_key_t parent_key;
        err = fs_new_key(fs, cwd, dir, &parent_key);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        err = fs_remove(fs, parent_key, basename);

        fs_delete_key(fs, parent_key);
    }

    return err;
}

static fernos_error_t fs_dump_tree_helper(file_sys_t *fs, void (*pf)(const char *, ...), fs_node_key_t nk,
        size_t depth) {
    fernos_error_t err;

    // This is hard on the stack, so I won't make it sooo big.
    char names_bufs[2][FS_MAX_FILENAME_LEN + 1];
    const size_t num_names_bufs = sizeof(names_bufs) / sizeof(names_bufs[0]);

    size_t name_index = 0;

    while (true) {
        err = fs_get_child_names(fs, nk, name_index, num_names_bufs, names_bufs); 

        // State mismatch should only be returned in the case where `nk` points to 
        // a file not a directory. This is OK, just don't print anything out.
        //
        // This is really here in the case where the first node will call this funciton on is
        // not a directory. The recursive case below makes sure only to recur when working with 
        // a subdirectory.
        if (err == FOS_E_STATE_MISMATCH) {
            return FOS_E_SUCCESS;
        }

        if (err != FOS_E_SUCCESS) {
            return err;
        }

        for (size_t i = 0; i < num_names_bufs; i++) {
            if (names_bufs[i][0] == '\0') {
                return FOS_E_SUCCESS; // We have process all names!
            }

            fs_node_key_t child_node_key;
            err = fs_new_key(fs, nk, names_bufs[i], &child_node_key);
            if (err != FOS_E_SUCCESS) {
                return err;
            }

            fs_node_info_t info;
            err = fs_get_node_info(fs, child_node_key, &info);

            // Color coded margin.
            for (size_t j = 0; j < depth; j++) {
                switch (j & 1) {
                case 0:
                    pf(".");
                    break;
                case 1:
                    pf(ANSI_GREEN_FG "." ANSI_RESET);
                    break;
                }
            }

            if (info.is_dir) {
                pf(ANSI_BRIGHT_BLUE_FG "%s\n" ANSI_RESET, names_bufs[i]);

                err = fs_dump_tree_helper(fs, pf, child_node_key, depth + 1);
                if (err != FOS_E_SUCCESS) {
                    return err;
                }
            } else {
                pf("%s (size=%u)\n", names_bufs[i], info.len);
            }

            fs_delete_key(fs, child_node_key);
        }

        name_index += num_names_bufs;
    }
}

void fs_dump_tree(file_sys_t *fs, void (*pf)(const char *, ...), fs_node_key_t cwd, const char *path) {
    fernos_error_t err;

    fs_node_key_t key;

    err = fs_new_key(fs, cwd, path, &key);
    if (err != FOS_E_SUCCESS) {
        return;
    }
    
    fs_dump_tree_helper(fs, pf, key, 0);

    fs_delete_key(fs, key);
}

