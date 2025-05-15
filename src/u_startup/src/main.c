
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"
#include "s_util/atomic.h"
#include "u_concur/mutex.h"
#include "s_util/constraints.h"

static void uprintf(const char *fmt, ...) {
    char buf[256];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    sc_term_put_s(buf);
}

void *other_main(void *arg);

mutex_t mut;

char str[3];

void *user_main(void *arg) {
    (void)arg;

    fernos_error_t err;

    str[0] = 'a';
    str[1] = 'a';
    str[2] = '\0';

    err = init_mutex(&mut);
    if (err != FOS_SUCCESS) {
        uprintf("Failed to create futex\n");
        while (1);
    }

    for (uint32_t i = 0; i < 4; i++) {
        err = sc_thread_spawn(NULL, other_main, (void *)i); 
        if (err != FOS_SUCCESS) {
            uprintf("Failed to spawn thread\n");
            while (1);
        }
    }

    proc_id_t cpid;

    err = sc_fork(&cpid);
    if (err != FOS_SUCCESS) {
        uprintf("Failed to fork\n"); 
        while (1);
    }

    if (cpid == FOS_MAX_PROCS) {
        uprintf("Hello from Child\n");
    } else {
        uprintf("Child spawned [%u]\n", cpid);
    }

    while (1);
}

void *other_main(void *arg) {
    uprintf("[%u] Starting Task\n", arg);

    mut_lock(&mut);

    str[0]++;
    str[1]++;
    sc_thread_sleep(10);

    uprintf("[%u] STR: %s\n", arg, str);

    mut_unlock(&mut);

    uprintf("[%u] Ending Task\n", arg);

    return NULL;
}


