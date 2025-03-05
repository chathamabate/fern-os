
#include "msys/page.h"
#include <stdint.h>

void init_page_table(page_table_entry_t *table) {
    for (uint32_t i = 0; i < 1024; i++) {
        table[i] = (page_table_entry_t){
            .present = 0  // Other fields should be set to 0.
        };
    }
}
