
#include "k_startup/tss.h"
#include "k_startup/page.h"
#include "k_sys/tss.h"
#include "s_util/misc.h"
#include "k_startup/gdt.h"

fernos_error_t init_global_tss(void) {
    task_state_segment_t *tss = (task_state_segment_t *)_tss_start;
    init_tss(tss);
    
    tss->iobm = sizeof(task_state_segment_t);
    tss->esp0 = KERNEL_STACK_END;
    tss->ss0 = KERNEL_DATA_SELECTOR;

    // NOTE: This expects that the tss is at 0x28 in the GDT!
    flush_tss(0x28);

    return FOS_SUCCESS;
}
