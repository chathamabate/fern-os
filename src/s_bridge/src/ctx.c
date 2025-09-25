
#include "s_bridge/ctx.h"

#include "k_sys/page.h"
#include "k_sys/intr.h"
#include "k_startup/vga_term.h"


intr_action_t gpf_action = NULL;

void set_gpf_action(intr_action_t ia) {
    gpf_action = ia;
}

intr_action_t pf_action = NULL;

void set_pf_action(intr_action_t ia) {
    pf_action = ia;
}

intr_action_t timer_action = NULL;

void set_timer_action(intr_action_t ta) {
    timer_action = ta;
}

syscall_action_t syscall_action = NULL;

void set_syscall_action(syscall_action_t sa) {
    syscall_action = sa;
}

intr_action_t irq1_action = NULL;

void set_irq1_action(intr_action_t ia) {
    irq1_action = ia;
}
