
#include "k_sys/page.h"
#include "s_util/misc.h"
#include <stdint.h>

static uint32_t nop_syscall_routine(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    return 0;
}


const phys_addr_t bridge_pd = NULL_PHYS_ADDR;
const uint32_t * const bridge_esp = NULL;

uint32_t (* const syscall_routine)(uint32_t, uint32_t, uint32_t, uint32_t) = nop_syscall_routine;

void set_bridge_context(phys_addr_t pd, const uint32_t *esp, 
        void (*r)(uint32_t, uint32_t, uint32_t, uint32_t)) {
    *(phys_addr_t *)&bridge_pd = pd; 
    *(const uint32_t **)&bridge_esp = esp;
    *(void **)&syscall_routine = (void *)r;
}


