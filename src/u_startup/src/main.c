
#include "u_startup/main.h"
#include "s_bridge/intr.h"


void user_main(void) {
    int i;
    trigger_syscall(0, (uint32_t)&i);
    while (1) {
    }
}
