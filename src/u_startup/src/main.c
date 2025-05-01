
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"

void *other_main(void *arg);

void *user_main(void *arg) {
    sc_thread_spawn(other_main, (void *)2);
    //sc_thread_spawn(other_main, (void *)1);
    while (1) {
        sc_term_put_s("Hello from OG\n");
        sc_thread_sleep(0);
    }
}

void *other_main(void *arg) {
    char S[32];
    str_fmt(S, "Hello from: %X\n", (uint32_t)arg);

    while (1) {
        sc_term_put_s(S);
        sc_thread_sleep(0);
    }
}


