
#include "k_sys/tss.h"
#include "s_util/str.h"

void init_tss(task_state_segment_t *tss) {
    mem_set(tss, 0, sizeof(task_state_segment_t));
}
