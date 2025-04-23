
#include "u_startup/main.h"
#include "s_bridge/intr.h"
#include "s_util/misc.h"

extern void tryout(void);
void user_main(void) {

    tryout();

    //trigger_syscall(0, 0);

    while (1) {
    }
}
