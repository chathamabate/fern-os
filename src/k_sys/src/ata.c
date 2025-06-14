
#include "k_sys/ata.h"

#include "k_bios_term/term.h"
#include "k_sys/io.h"

#include <stdint.h>

static void wait400(void) {
    for (uint32_t i = 0; i < 14; i++) {
        uint8_t alt_status = inb(ATA_REG_ALT_STAT);
        term_put_fmt_s("Alt Status: 0x%X\n", alt_status);
    }
}

void run_ata_test(void) {
    // Right now interrupts are disabled btw... Might want to change this later!!
    term_put_s("Hello From ata code\n");

    outb(ATA_REG_DEV_HD, 0xA0); // Select Master Drive.

    // Clear registers.
    outb(ATA_REG_SEC_COUNT, 0x0);
    outb(ATA_REG_LBA0, 0x0);
    outb(ATA_REG_LBA1, 0x0);
    outb(ATA_REG_LBA2, 0x0);

    // Write out  IDENTIFY command.
    outb(ATA_REG_COMMAND, 0xEC);

    wait400();
    lock_up();


    

    uint8_t dev = inb(ATA_REG_DEV_HD);
    term_put_fmt_s("Dev HD Reg: 0x%X\n", dev);

    lock_up();

    //outb(ATA_REG_DEV_HD_DEV_MASK | ATA_REG_DEV_HD_DEV_MASK);

    // We could try attaching to the master drive first, as ChatGPT tells me.

    const uint8_t READY =  ATA_REG_STATUS_DRDY_OFF | ATA_REG_STATUS_BSY_MASK;
    uint8_t alt_status = inb(ATA_REG_ALT_STAT);

    term_put_fmt_s("Alt Status: 0x%X\n", alt_status);

    /*
    if ((alt_status & READY) ==  READY) {
        term_put_s("Disk Ready\n");
    } else {
        term_put_s("Disk not Ready\n");
        lock_up();
    }
    */

    // Check power mode.
    outb(ATA_REG_COMMAND, 0xE5);
    wait400();
    
     alt_status = inb(ATA_REG_ALT_STAT);
    term_put_fmt_s("Alt Status: 0x%X\n", alt_status);


    wait400();

    lock_up();
}
