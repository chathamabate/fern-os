
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"

extern void tryout(void);
void user_main(void) {
    tryout();
    term_put_s("Hello from user\n");
    uint32_t ret = trigger_syscall(3, (void *)4);
    term_put_fmt_s("RetVal: %u\n", ret);
    while (1);
}


