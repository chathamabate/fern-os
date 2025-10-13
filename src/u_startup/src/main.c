
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_fs.h"
#include "u_startup/syscall_vga_cd.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"

#include "s_elf/elf32.h"
#include <stdarg.h>

proc_exit_status_t user_main(void) {
    // User code here! 
    
    fernos_error_t err;

    handle_t cd;
    err = sc_vga_cd_open(&cd);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    sc_set_out_handle(cd);

    err = setup_default_simple_heap(USER_MMP);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

    fs_node_info_t info; 
    err = sc_fs_get_info("/core_apps/dummy_app", &info);
    if (err != FOS_E_SUCCESS) {
        return PROC_ES_FAILURE;
    }

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

    for (size_t i = 0; i < elf_header->num_program_headers; i++) {
        // Alright so we are looking for sections marked loadable,
        // the difference between the file and mem size is what should be zero'd
        // according to deepseek. Ok, the real hard part is how arguments should be handled!
        // We could always just give it as another ro section???
        // I guess IDK for sure...
        // I want to load these sections into memory and hand them to the kernel directly so that
        // the kernel doesn't need to do any slow file reading.
        // I guess IDK for sure...
        // Why not just define some specific arguments section that's outside the app area??
        // Not the worst idea in the world tbh.
        //
        // Ok, now there's an args area...
        // So what do we do now??? I don't really know tbh.
        
        sc_out_write_fmt_s(
            "PG[%u] TYPE: 0x%X FLAGS: 0x%X VADDR: 0x%X FSIZE: 0x%X MSIZE: 0x%X\n",
            i,
            pg_hdr_table[i].type,
            pg_hdr_table[i].flags,
            pg_hdr_table[i].vaddr,
            pg_hdr_table[i].size_in_file,
            pg_hdr_table[i].size_in_mem
        ); 
    }

    for (size_t i = 0; i < elf_header->num_section_headers; i++) {
        if (sect_hdr_table[i].type != ELF32_SEC_TYPE_NULL) {
            sc_out_write_fmt_s("%s VADDR: 0x%X OFFSET: 0x%X SIZE: 0x%X\n", 
                    &(names[sect_hdr_table[i].name_offset]),
                    sect_hdr_table[i].vaddr,
                    sect_hdr_table[i].offset,
                    sect_hdr_table[i].size);
        }
    }

    // Damn, it's kinda cool that this all works!

    //sc_out_write_fmt_s("%s\n", names);


    da_free(elf_data);
    sc_handle_close(fh);
    sc_handle_close(cd);

    return PROC_ES_SUCCESS;
}
