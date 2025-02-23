
#ifndef MSYS_PAGE_H
#define MSYS_PAGE_H

#include <stdint.h>

struct _page_table_entry_t {
    uint32_t present : 1;
    uint32_t read_or_write : 1;
    uint32_t super_or_user : 1;    
    uint32_t pad0 : 2;
    uint32_t accessed : 1;
    uint32_t dirty : 1;
    uint32_t pad1 : 2;
    uint32_t avail : 3;
    uint32_t addr : 20;
} __attribute__ ((packed));

typedef struct _page_table_entry_t page_table_entry_t;

#endif
