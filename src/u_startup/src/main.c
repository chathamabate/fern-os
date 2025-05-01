
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"

void *other_main(void *arg);

void *user_main(void *arg) {
    char S[32];
    thread_id_t tid;
    
    fernos_error_t err = sc_thread_spawn(&tid, other_main, (void *)2);
    if (err != FOS_SUCCESS) {
        sc_term_put_s("Failed to spawn thread\n");
    } else {
        str_fmt(S, "Spawned ID: %X\n", tid);
        sc_term_put_s(S);
    }

    while (1) {
        sc_thread_sleep(0);
    }
}

void *other_main(void *arg) {

    while (1) {
        sc_thread_sleep(0);
    }
}


