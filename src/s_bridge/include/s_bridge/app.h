
#pragma once

#include "s_mem/allocator.h"

/**
 * This is a structure which provides the kernel with all needed information for loading and
 * executing a user application. It's essentially a denser elf file stored entirely in memory.
 */
typedef struct _user_app_t user_app_t;
typedef struct _user_app_area_entry_t user_app_area_entry_t;

struct _user_app_area_entry_t {
    /**
     * When false, this entry is not in use.
     */
    bool occupied;

    /**
     * Whether or not this area should be marked writeable.
     *
     * (NOTE: This is more of a suggestion to the kernel, it is possible all sections are always
     * marked writeable)
     */
    bool writeable;

    /**
     * Where to load this section in memory of the user process. This must be within the 
     * FOS_APP_AREA range.
     */
    void *load_position;

    /**
     * The size this area will occupy in the user process.
     */
    size_t area_size;

    /**
     * Given bytes to be loaded directly into the user process.
     * (Can be NULL if `given_size` = 0)
     */
    const void *given;

    /**
     * Size of the buffer pointed to by `given`. Must be <= `area_size`.
     * Excess bytes in the area will be zero'd.
     */
    size_t given_size;
};

/**
 * Using a static max to save a small amount of time with copying area information from userspace
 * to kernel space. Really, apps should only need 3 areas (text + ro, data, bss).
 */
#define FOS_MAX_APP_AREAS (10U)

struct _user_app_t {
    /**
     * Allocator used to alloc this app.
     */
    allocator_t *al; 

    /**
     * Where the entry point of the application is. Must be in the FOS_APP_AREA.
     */
    const void *entry;

    /**
     * Areas to load for this app. Only occupied entries are interpreted.
     */
    user_app_area_entry_t areas[FOS_MAX_APP_AREAS];
};

/**
 * Create a new empty user app, all entries will be marked unoccupied.
 * `entry` will be set to NULL.
 *
 * Returns NULL if there isn't enough memory.
 */
user_app_t *new_user_app(allocator_t *al);

static inline user_app_t *new_da_user_app(void) {
    return new_user_app(get_default_allocator());
}

/**
 * Free a user app structure.
 *
 * Make sure that when the user app structure is completely copied into kernel space that the 
 * allocator field is set to the kernel allocator! Otherwise, the user allocator will attempt
 * to be used during deletion!
 */
void delete_user_app(user_app_t *ua);

/**
 * Create a new arguments block from a list of args.
 *
 * On success, FOS_E_SUCCESS is returned, a pointer to the new block is written to `*out` 
 * and the length of the args block is written to `*out_len`.
 *
 * NOTE: That if no args are given, Success will still be returned and NULL/0 will be written out.
 *
 * The args block will start with a uint32_t[], each entry in this array will be a byte-index
 * into the args block of an argument. This starting sequences of indeces will be terminated 
 * with 0. For example, below would be a valid args block for arguments "arg1", "arg2".
 *
 * 0xC, 0, 0, 0,            (Index 12 -> "arg1")
 * 0x1, 0x1, 0, 0,          (Index 17 -> "arg2")
 * 0, 0, 0, 0,              (NT = No more arguments)
 * 'a', 'r', 'g', '1',      "arg1"
 * 0, 'a', 'r', 'g',        "arg2"
 * '2', 0   
 *
 * `arg_block_size` = 22 (or 0x16)
 *
 * At load time, this block of data will be copied directly into the user process. However,
 * first, each index will be replace with the correct virtual address.
 *
 * A pointer to the beginning of this block is what is provided to the user main funciton.
 * (i.e. of type: const char * const *)
 */
fernos_error_t new_args_block(allocator_t *al, const char * const *args, size_t num_args, 
        const void **out, size_t *out_len);

static inline fernos_error_t new_da_args_block(const char * const *args, size_t num_args, 
        const void **out, size_t *out_len) {
    return new_args_block(get_default_allocator(), args, num_args, out, out_len);
}

