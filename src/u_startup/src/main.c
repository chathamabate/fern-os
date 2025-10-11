
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/syscall_vga_cd.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_util/constraints.h"

#include "s_elf/elf32.h"
#include <stdarg.h>

static fernos_error_t setup_user_heap(void) {
    allocator_t *al = new_simple_heap_allocator(
        (simple_heap_attrs_t) {
            .start = (void *)FOS_FREE_AREA_START,
            .end = (const void *)(FOS_FREE_AREA_END),

            .mmp = (mem_manage_pair_t) {
                .request_mem = sc_mem_request,
                .return_mem = sc_mem_return
            },

            .small_fl_cutoff = 0x100,
            .small_fl_search_amt = 0x20,
            .large_fl_search_amt = 0x200
        }
    );

    if (!al) {
        return FOS_E_UNKNWON_ERROR;
    }

    set_default_allocator(al);

    return FOS_E_SUCCESS;
}

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_set_out_handle(cd);

    err = setup_user_heap();
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    fs_node_info_t info; 
    err = sc_fs_get_info("/core_apps/dummy_app", &info);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    // 133 KBs is actually bigger than I expected.
    sc_out_write_fmt_s("ELF File Size: %u\n", info.len);

    handle_t fh;
    err = sc_fs_open("/core_apps/dummy_app", &fh);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    uint8_t *elf_data = da_malloc(info.len);
    if (!elf_data) {
        return PROC_ES_FAILURE;
    }

    err = sc_handle_read_full(fh, elf_data, info.len);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    // Can we link against an ELF file without loading though???
    // My head kinda hurts still though tbh.

    elf32_header_t *elf_header = (elf32_header_t *)elf_data;
    elf32_program_header_t *pg_hdr_table = (elf32_program_header_t *)&(elf_data[elf_header->program_header_table]);
    elf32_section_header_t *sect_hdr_table = (elf32_section_header_t *)&(elf_data[elf_header->section_header_table]);
    elf32_section_header_t *sect_name_hdr = &(sect_hdr_table[elf_header->section_names_header_index]);
    const char *names = (const char *)&(elf_data[sect_name_hdr->offset]);

    sc_out_write_fmt_s("PG Table Offset: 0x%X\n", elf_header->program_header_table);
    sc_out_write_fmt_s("Num PG Headers: %u\n", elf_header->num_program_headers);
    sc_out_write_fmt_s("Sect Table Offset: 0x%X\n", elf_header->section_header_table);
    sc_out_write_fmt_s("Num Sect Headers: %u\n", elf_header->num_section_headers);
    sc_out_write_fmt_s("Name Section Index: %u\n", elf_header->section_names_header_index);

    for (size_t i = 0; i < elf_header->num_section_headers; i++) {
        sc_out_write_fmt_s("%s\n", &(names[sect_hdr_table[i].name_offset]));
    }

    // Damn, it's kinda cool that this all works!

    //sc_out_write_fmt_s("%s\n", names);


    da_free(elf_data);
    sc_handle_close(fh);
    sc_handle_close(cd);

    return PROC_ES_SUCCESS;
}
