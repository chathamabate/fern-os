
#include "k_sys/ata.h"

#include "k_bios_term/term.h"
#include "k_sys/io.h"

static void wait400(void) {
    for (uint32_t i = 0; i < 14; i++) {
        inb(ATA_REG_ALT_STAT);
    }
}

void run_ata_test(void) {
    // Right now interrupts are disabled btw...
    term_put_s("Hello From ata code\n");

    outb();

    wait400();

    lock_up();
}
