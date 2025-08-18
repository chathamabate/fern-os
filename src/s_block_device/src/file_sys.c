
#include "s_block_device/file_sys.h"

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

    for (uint32_t i = 0; i < FS_MAX_FILENAME_LEN; i++) {
        char c = fn[i];

        // Reached the end of the string without issues!
        if (c == '\0') {
            return true;
        }

        // Found a bad character!
        if (!fn_char_map[(uint8_t)c]) {
            return false;
        }
    }

    // Only return true if we actually reached the end of the string!
    return fn[FS_MAX_FILENAME_LEN] == '\0';
}
