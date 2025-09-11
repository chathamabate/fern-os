
#include "k_startup/handle.h"

void clear_handle_table(id_table_t *handle_table) {
    for (size_t h = 0; h < FOS_MAX_HANDLES_PER_PROC; h++) {
        delete_handle_state(idtb_get(handle_table, h));
        idtb_push_id(handle_table, h);
    }
}
