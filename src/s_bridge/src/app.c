

#include "s_bridge/app.h"
#include "s_util/str.h"

void delete_user_app(user_app_t *ua) {
    for (size_t i = 0; i < FOS_MAX_APP_AREAS; i++) {
        if (ua->areas[i].occupied && ua->areas[i].given_size > 0) {
            al_free(ua->al, (void *)(ua->areas[i].given));
        }
    }

    al_free(ua->al, ua);
}

fernos_error_t new_args_block(allocator_t *al, const char * const *args, size_t num_args, 
        const void **out, size_t *out_len) {
    if (!out || !out_len) {
        return FOS_E_BAD_ARGS;
    }

    if (!args || num_args == 0) {
        *out = NULL;
        *out_len = 0;

        return FOS_E_SUCCESS;
    }

    // Ok, first we need to calc the size of the full block.
    size_t str_area_size = 0;
    for (size_t i = 0; i < num_args; i++) {
        if (!args[i]) {
            return FOS_E_BAD_ARGS;
        }

        str_area_size += str_len(args[i]) + 1;
    }

    const size_t tbl_area_size = ((num_args + 1) * sizeof(uint32_t));
    const size_t block_size = tbl_area_size + str_area_size;

    void * const block = al_malloc(al, block_size);
    if (!block) {
        return FOS_E_NO_MEM;
    }

    uint32_t * const tbl_area = (uint32_t *)block; // where offsets are placed.
    char * const str_area = (char *)(tbl_area + num_args + 1); // Where actual strings are placed.

    // Ok, now for the placement!
    size_t next_str_ind = 0;
    for (size_t i = 0; i < num_args; i++) {
        tbl_area[i] = (uint32_t)next_str_ind; 
        str_cpy(str_area + next_str_ind, args[i]);
        next_str_ind += str_len(args[i]) + 1;
    }

    tbl_area[num_args] = 0; // Terminate the table area!

    *out = block;
    *out_len = block_size;

    return FOS_E_SUCCESS;
}
