
#include "k_startup/tss.h"
#include "k_startup/page.h"
#include "k_sys/tss.h"
#include "s_util/misc.h"
#include "k_startup/gdt.h"
#include "s_util/constraints.h"

fernos_error_t init_global_tss(void) {
    task_state_segment_t *tss = (task_state_segment_t *)_tss_start;
    init_tss(tss);
    
    // NOTE: Remember, the last 4 bytes always holds the kernel page table!
    tss->esp0 = FOS_KERNEL_STACK_END - sizeof(uint32_t);
    tss->ss0 = KERNEL_DATA_SELECTOR;
    tss->iobm = sizeof(task_state_segment_t);

    flush_tss(TSS_SELECTOR);

    return FOS_SUCCESS;
}
