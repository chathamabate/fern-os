
#include "s_data/id_table.h"
#include "s_mem/allocator.h"
#include "s_util/ansii.h"
#include "s_util/misc.h"

#define IDTB_STARTING_CAP 0x4

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

    idtb->al_head = mc; // allocated list starts as empty!
    idtb->fl_head = 0;

    // Set up free list.
    for (id_t id = 0; id < starting_cap; id++) {
        tbl[id].allocated = false;

        tbl[id].next = id + 1;
        tbl[id].prev = id - 1;

        tbl[id].data = NULL;
    }

    tbl[0].prev = mc;
    tbl[starting_cap - 1].next = mc;

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

/**
 * Resize an ID Table to a given new capacity.
 *
 * (This can be called regardless of whether or not the free list is currently empty)
 *
 * Fails if the the capacity is invalid OR if there are insufficient resources.
 *
 * In case of failure, the table is left in its original state.
 */
static fernos_error_t idtb_resize(id_table_t *idtb, uint32_t new_cap) {
    if (new_cap <= idtb->cap) {
        return FOS_SUCCESS;
    }

    if (new_cap > idtb->max_cap) {
        return FOS_BAD_ARGS;
    }

    id_table_entry_t *new_tbl = al_realloc(idtb->al, idtb->tbl,
            new_cap * sizeof(id_table_entry_t));

    if (!new_tbl) {
        return FOS_NO_MEM;
    }

    const id_t NULL_ID = idtb_null_id(idtb);

    for (id_t new_id = idtb->cap; new_id < new_cap; new_id++) {
        new_tbl[new_id].allocated = false;

        new_tbl[new_id].next = new_id + 1;
        new_tbl[new_id].prev = new_id - 1;
        
        new_tbl[new_id].data = NULL;
    }

    new_tbl[idtb->cap].prev = NULL_ID;

    new_tbl[new_cap - 1].next = idtb->fl_head;
    if (idtb->fl_head != NULL_ID) {
        new_tbl[idtb->fl_head].prev = new_cap - 1;
    }

    idtb->fl_head = idtb->cap;

    idtb->cap = new_cap;
    idtb->tbl = new_tbl;

    return FOS_SUCCESS;
}

fernos_error_t idtb_request_id(id_table_t *idtb, id_t id) {
    const id_t NULL_ID = idtb_null_id(idtb);

    if (id >= idtb->max_cap) {
        return FOS_INVALID_INDEX;
    }

    if (id >= idtb->cap) {
        uint32_t new_cap = idtb->cap * 2;
        if (new_cap <= idtb->cap || new_cap > idtb->max_cap) {
            new_cap = idtb->max_cap;
        }

        // If doubling the size is still not enough, Just set the new cap to the id + 1 directly.
        if (id >= new_cap) {
            new_cap = id + 1;
        }

        // Do we need to resize??
        fernos_error_t err = idtb_resize(idtb, new_cap);

        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    // If we make it here, we know id < cap.
    
    if (idtb->tbl[id].allocated) {
        return FOS_ALREADY_ALLOCATED;
    }

    // Ok, now we know we can pop the id!
    // First let's remove it from the free list!
   
    id_t next = idtb->tbl[id].next;
    id_t prev = idtb->tbl[id].prev;

    if (next != NULL_ID) {
        idtb->tbl[next].prev = prev;
    }

    if (prev != NULL_ID) {
        idtb->tbl[prev].next = next;
    } else {
        idtb->fl_head = next;
    }

    // Now place the id in the allocated list!
    
    id_t al_head = idtb->al_head;

    if (al_head != NULL_ID) {
        idtb->tbl[al_head].prev = id;
    }

    idtb->tbl[id].next = al_head;
    idtb->al_head = id;

    // Finally set as allocated!

    idtb->tbl[id].allocated = true;

    return FOS_SUCCESS;
}

id_t idtb_pop_id(id_table_t *idtb) {
    const id_t NULL_ID = idtb_null_id(idtb);
    id_t id = NULL_ID;

    if (idtb->fl_head != NULL_ID) {
        id = idtb->fl_head;
    } else {
        id = idtb->cap;
    }

    if (idtb_request_id(idtb, id) != FOS_SUCCESS) {
        id = NULL_ID;
    }

    return id;
}

void idtb_push_id(id_table_t *idtb, id_t id) {
    if (id >= idtb->cap || !(idtb->tbl[id].allocated)) {
        return;
    }

    const id_t NULL_ID = idtb_null_id(idtb);

    /*
     * Kinda like pop, but other directrion.
     *
     * Somewhat different because the pushed id can be at any location in the allocated list.
     */

    // Remove from allocate list First.
    
    id_t next = idtb->tbl[id].next;
    id_t prev = idtb->tbl[id].prev;

    if (next != NULL_ID) {
        idtb->tbl[next].prev = prev;
    }

    if (prev != NULL_ID) {
        idtb->tbl[prev].next = next;
    } else {
        idtb->al_head = next; // No previous means new head!
    }

    // Now place our cell in the free list.

    idtb->tbl[id].allocated = false;

    idtb->tbl[id].next = idtb->fl_head;
    if (idtb->fl_head != NULL_ID) {
        idtb->tbl[idtb->fl_head].prev = id;
    }

    idtb->tbl[id].prev = NULL_ID;
    idtb->tbl[id].data = NULL;

    idtb->fl_head = id;
}

void idtb_reset_iterator(id_table_t *idtb) {
    idtb->iter = idtb->al_head;
}

id_t idtb_get_iter(id_table_t *idtb) {
    return idtb->iter;
}

id_t idtb_next(id_table_t *idtb) {
    const id_t NULL_ID = idtb_null_id(idtb); 

    id_t iter = idtb->iter;

    if (iter != NULL_ID) {
        idtb->iter = idtb->tbl[iter].next;
    }

    return idtb->iter;
}

void idtb_dump(id_table_t *idtb, void (*pf)(const char *fmt, ...)) {
    pf("ID Table\n");

    dump_hex_pairs(pf,
        "Cap", idtb->cap,
        "Max Cap", idtb->max_cap
    );
    pf("\n");

    dump_hex_pairs(pf,
        "Alloc List Head", idtb->al_head,
        "Free List Head", idtb->fl_head
    );
    pf("\n");

    pf("------ Table ------\n");
    for (id_t id = 0; id < idtb->cap; id++) {
        pf("[" ANSII_CYAN_FG "0x%X" ANSII_RESET "] ", id);

        if (idtb->tbl[id].allocated) {
            pf(ANSII_RED_FG "ALLOCATED (0x%X) " ANSII_RESET, idtb->tbl[id].data);
        } else {
            pf(ANSII_GREEN_FG "FREE " ANSII_RESET);
        }

        dump_hex_pairs(pf, 
            "Next", idtb->tbl[id].next,
            "Prev", idtb->tbl[id].prev
        );

        pf("\n");
    }
    pf("-------------------\n");
}
