
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"

extern void tryout(void);


/*
void other_func(void) {
    uint32_t t[1024];
    term_put_s("HELLO\n");
}
*/

void user_main(void) {
    tryout();
    /*
    uint32_t x = 0;
    while (1) {
        x += 2;
        other_func();
    }
    */
}


