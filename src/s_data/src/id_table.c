
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

id_t idtb_pop_id(id_table_t *idtb) {
    const id_t NULL_ID = idtb_null_id(idtb);

    if (idtb->fl_head == NULL_ID && idtb->cap < idtb->max_cap) {
        // In this case we attempt a resize.
        
        // We need a resize!
        uint32_t new_cap = idtb->cap * 2;  
        if (new_cap <= idtb->cap || new_cap > idtb->max_cap) {
            new_cap = idtb->max_cap;
        }

        // I believe if we make it here, it is gauranteed that new_cap > cap.

        id_table_entry_t *new_tbl = al_realloc(idtb->al, idtb->tbl,
                new_cap * sizeof(id_table_entry_t));

        // Did our realloc succeed?
        if (new_tbl) {
            // Remeber, there is a gaurantee that our free list is empty if we are reallocing!
            
            for (id_t new_id = idtb->cap; new_id < new_cap; new_id++) {
                new_tbl[new_id].allocated = false;

                new_tbl[new_id].next = new_id + 1;
                new_tbl[new_id].prev = new_id - 1;
                
                new_tbl[new_id].data = NULL;
            }

            new_tbl[idtb->cap].prev = NULL_ID;
            new_tbl[new_cap - 1].next = NULL_ID;

            idtb->fl_head = idtb->cap;

            idtb->cap = new_cap;
            idtb->tbl = new_tbl;
        }     

        // Remember if the realloc fails, that's OK. 
        // The original table should be left untouched.
    }

    if (idtb->fl_head == NULL_ID) {
        return NULL_ID;
    }

    /*
     * Somewhat confusing, but we need to take the head of the free list, and move it to the 
     * allocated list.
     */
    
    id_t new_al_head = idtb->fl_head; // Gauranteed NOT-NULL.
    id_t new_fl_head = idtb->tbl[new_al_head].next;
    id_t old_al_head = idtb->al_head;

    // Set up our new allocated cell.
    idtb->tbl[new_al_head].allocated = true;
    idtb->tbl[new_al_head].next = old_al_head;
    // Prev and data are gauranteed to already be NULL.

    if (old_al_head != NULL_ID) {
        idtb->tbl[old_al_head].prev = new_al_head;
    }
    idtb->al_head = new_al_head;

    if (new_fl_head != NULL_ID) {
        idtb->tbl[new_fl_head].prev = NULL_ID;
    }
    idtb->fl_head = new_fl_head;

    return new_al_head;
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

id_t idtb_next(id_table_t *idtb) {
    const id_t NULL_ID = idtb_null_id(idtb); 

    id_t iter = idtb->iter;

    if (iter != NULL_ID) {
        idtb->iter = idtb->tbl[iter].next;
    }

    return iter;
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
