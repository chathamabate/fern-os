

#include "u_startup/syscall_fs.h"
#include "u_startup/syscall.h"
#include "s_bridge/shared_defs.h"
#include "s_util/str.h"
#include "s_util/misc.h"
#include "s_util/elf.h"

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

/**
 * This expects that `user_app` points to an ampty user app structure.
 * (One where the allocator is set, but all area entries are unoccupied)
 */
static fernos_error_t sc_fs_parse_elf32_helper(handle_t fh, size_t file_len, user_app_t *user_app) {
    if (!user_app) {
        return FOS_E_BAD_ARGS;
    }

    if (file_len < sizeof(elf32_header_t)) {
        return FOS_E_EMPTY;
    }

    elf32_header_t elf32_header;
    PROP_ERR(sc_handle_read_full(fh, &elf32_header, sizeof(elf32_header_t)));

    // Must be 32-bit little-endian targeting x86 with an entry point!
    if (elf32_header.header_magic != ELF_HEADER_MAGIC || 
            elf32_header.cls != 1 || elf32_header.endian != 1 ||
            elf32_header.machine != 0x03 || 
            elf32_header.this_header_size != sizeof(elf32_header_t) ||
            elf32_header.program_header_size != sizeof(elf32_program_header_t) ||
            elf32_header.section_header_size != sizeof(elf32_section_header_t)) {
        return FOS_E_STATE_MISMATCH;
    }

    const size_t num_pg_headers = elf32_header.num_program_headers;
    if (file_len < elf32_header.program_header_table + (num_pg_headers * sizeof(elf32_program_header_t))) {
        return FOS_E_EMPTY; // File doesn't contain expected program header table.
    }

    // Set entry point!
    user_app->entry = elf32_header.entry;
    // All area entries will start as unoccupied.

    fernos_error_t err;

    size_t area_ind = 0;
    for (size_t i = 0; i < num_pg_headers; i++) {
        elf32_program_header_t pg_header; // we'll read one program header at a time.
        
        PROP_ERR(sc_fs_seek(fh, elf32_header.program_header_table + (i * sizeof(elf32_program_header_t))));
        PROP_ERR(sc_handle_read_full(fh, &pg_header, sizeof(elf32_program_header_t)));

        // We only care about loadable segments for now!
        if (pg_header.type == ELF32_SEG_TYPE_LOADABLE) {
            if (area_ind >= FOS_MAX_APP_AREAS) {
                return FOS_E_NO_SPACE;
            }

            // Ok, now we gotta actually read the given data from the file 
            // (if there is any such data)
            
            void *given = NULL;
            if (pg_header.size_in_file > 0) {
                if (file_len < pg_header.offset + pg_header.size_in_file) {
                    return FOS_E_EMPTY; // This elf file doesn't hold the program segment
                                        // it says it does!
                }

                PROP_ERR(sc_fs_seek(fh, pg_header.offset));
                given = al_malloc(user_app->al, pg_header.size_in_file); 
                if (!given) {
                    return FOS_E_NO_MEM;
                }

                err = sc_handle_read_full(fh, given, pg_header.size_in_file);
                if (err != FOS_E_SUCCESS) {
                    al_free(user_app->al, given);
                    return err;
                }
            }

            // Place everything in the user_app structure (Regardless of whether there is given
            // data or not)

            user_app_area_entry_t *area_entry = user_app->areas + (area_ind++);

            area_entry->occupied = true;
            area_entry->given = given;
            area_entry->given_size = pg_header.size_in_file;
            area_entry->area_size = pg_header.size_in_mem;
            area_entry->load_position = pg_header.vaddr;
            area_entry->writeable = (pg_header.flags & ELF32_SEG_FLAG_WRITEABLE) == ELF32_SEG_FLAG_WRITEABLE;
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t sc_fs_parse_elf32(allocator_t *al, const char *path, user_app_t **ua) {
    fernos_error_t err;

    if (!al || !path || !ua) {
        return FOS_E_BAD_ARGS;
    }

    fs_node_info_t info;
    sc_fs_get_info(path, &info);
    
    if (info.is_dir) {
        return FOS_E_STATE_MISMATCH;
    }

    user_app_t *user_app = new_user_app(al);
    if (!user_app) {
        return FOS_E_NO_MEM;
    }

    const size_t file_len = info.len;

    handle_t fh; 

    err = sc_fs_open(path, &fh);
    if (err != FOS_E_SUCCESS) {
        delete_user_app(user_app);
        return err;
    }

    // fh needs closing after this point always!

    err = sc_fs_parse_elf32_helper(fh, file_len, user_app);

    if (err == FOS_E_SUCCESS) {
        *ua = user_app;
    } else {
        delete_user_app(user_app);
        *ua = NULL;
    }

    sc_handle_close(fh);

    return err;
}
