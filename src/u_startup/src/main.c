
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"

uint8_t __tmp_stack[0x1000];
extern void tryout(void);
void user_main(void) {
    while (1);
}


