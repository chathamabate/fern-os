
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"
#include "s_util/atomic.h"

static void uprintf(const char *fmt, ...) {
    char buf[256];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    sc_term_put_s(buf);
}

void *other_main(void *arg);

spinlock_t spl;
uint32_t val;

void *user_main(void *arg) {
    (void)arg;

    uprintf("HELLO\n");
    while (1);

    val = 0;
    init_spinlock(&spl);

    fernos_error_t err = sc_thread_spawn(NULL, other_main, NULL);
    if (err != FOS_SUCCESS) {
        uprintf("Failed to spawn thread\n");
        while (1);
    }


    while (1) {
        spl_lock(&spl);
        uprintf("Hello from main thread\n");
        spl_unlock(&spl);
        uprintf("Hello from main thread\n");
    }
}

void *other_main(void *arg) {
    sc_thread_sleep(3);
    while (1) {
        spl_lock(&spl);
        uprintf("Hello from other thread\n");
        spl_unlock(&spl);

        // Woo this kinda works!!
        // Now for the "futex" bullshit.. whatever that means tbh...
        uprintf("Hello from other thread\n");
    }
}


