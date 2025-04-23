
#include "k_startup/tss.h"
#include "k_sys/tss.h"
#include "s_util/misc.h"

static uint8_t tss_esp[M_4K] __attribute__((aligned(M_4K)));

fernos_error_t init_global_tss(void) {
    task_state_segment_t *tss = (task_state_segment_t *)_tss_start;
    init_tss(tss);
    
    tss->ss0 = 0x10; // kernel data.
    tss->esp0 = (uint32_t)tss_esp;

    return FOS_SUCCESS;
}
