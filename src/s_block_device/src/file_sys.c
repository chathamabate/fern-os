
#include "s_block_device/file_sys.h"
#include "s_util/str.h"

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
