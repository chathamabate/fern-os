
#include "s_util/misc.h"
#include <stdint.h>

static uint32_t nop_syscall_routine(uint32_t arg0, uint32_t arg1, uint32_t arg2, 
        uint32_t arg3, uint32_t arg4) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;

    return 0;
}

static uint32_t (*syscall_routine)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = nop_syscall_routine;

void set_syscall_routine(uint32_t (*r)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) {
    syscall_routine = r;
}

void do_smt(void) {

}
