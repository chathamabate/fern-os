
#include "k_startup/tss.h"
#include "k_sys/tss.h"
#include "s_util/misc.h"

/**
 * I think the problem here is that this is not in the user space!
 */
static uint8_t tss_stack[M_4K] __attribute__((aligned(M_4K)));

fernos_error_t init_global_tss(void) {
    task_state_segment_t *tss = (task_state_segment_t *)_tss_start;
    init_tss(tss);
    
    tss->iobm = sizeof(task_state_segment_t);
    tss->ss0 = 0x10; // kernel data.
    tss->esp0 = (uint32_t)tss_stack + sizeof(tss_stack);

    // NOTE: This expects that the tss is at 0x28 in the GDT!
    flush_tss(0x28);

    return FOS_SUCCESS;
}
