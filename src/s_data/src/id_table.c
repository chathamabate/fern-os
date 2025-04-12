
#include "s_data/id_table.h"
#include "s_mem/allocator.h"

#define IDTB_STARTING_CAP 0x10U

id_table_t *new_id_table(allocator_t *al, uint32_t mc) {
    if (mc == 0 || !al) {
        return NULL;
    }

    id_table_t *idtb = al_malloc(al, sizeof(id_table_t));
    if (!idtb) {
        return NULL;
    }

    uint32_t starting_cap = IDTB_STARTING_CAP;
    if (mc < starting_cap) {
        starting_cap = mc;
    }

    id_table_entry_t *tbl = al_malloc(al, sizeof(id_table_entry_t) * starting_cap);
    if (!tbl) {
        al_free(al, idtb);
        return NULL;
    }

    // Success!

    idtb->al = al;
    idtb->max_cap = mc;
    idtb->cap = starting_cap;
    idtb->fl_head = 0;

    // Set up free list.
    for (id_t id = 0; id < starting_cap; id++) {
        tbl[id].allocated = false;
        tbl[id].cell.next = id + 1;
    }

    // Last entry should point to NULL, it will be the end of the inital free list.
    tbl[starting_cap - 1].cell.next = mc;

    idtb->tbl = tbl;

    return idtb;
}

void delete_id_table(id_table_t *idtb) {
    if (!idtb) {
        return;
    }

    allocator_t *al = idtb->al;

    al_free(al, idtb->tbl);
    al_free(al, idtb);
}

id_t idtb_pop_id(id_table_t *idtb) {

}

void idtb_push_id(id_table_t *idtb, id_t id) {
    
}
