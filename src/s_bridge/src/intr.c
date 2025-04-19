
#include "s_bridge/intr.h"
#include "k_sys/page.h"

const phys_addr_t intr_pd;
const uint32_t * const intr_esp;

void set_intr_ctx(phys_addr_t pd, const uint32_t *esp) {
    *(phys_addr_t *)&intr_pd = pd;
    *(const uint32_t **)&intr_esp = esp;
}

void (* const lock_up_action)(void) = NULL;

void set_lock_up_action(void (*action)(void)) {
    *(void (**)(void))&lock_up_action = action;
}
