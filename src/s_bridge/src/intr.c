
#include "s_bridge/intr.h"
#include "k_bios_term/term.h"
#include "k_sys/page.h"
#include "k_sys/intr.h"

const phys_addr_t intr_pd = NULL_PHYS_ADDR;
const uint32_t * const intr_esp = NULL;

void set_intr_ctx(phys_addr_t pd, const uint32_t *esp) {
    *(phys_addr_t *)&intr_pd = pd;
    *(const uint32_t **)&intr_esp = esp;
}

void _lock_up_handler(void) {
    disable_intrs(); 

    out_bios_vga(
        vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK),
        "Interrupt occured with no handler!"
    );

    lock_up();
}

const syscall_action_t syscall_action = NULL;

void set_syscall_action(syscall_action_t sa) {
    *(syscall_action_t *)&syscall_action = sa;
}

const timer_action_t timer_action = NULL;

void set_timer_action(timer_action_t ta) {
    *(timer_action_t *)&timer_action = ta; 
}


