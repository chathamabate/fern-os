
#include "k_sys/ata.h"

#include "k_bios_term/term.h"
#include "k_sys/io.h"

#include <stdint.h>

/**
 * This should wait around 400ns which is the recommended pause to wait
 * before assuming your command has been acknowledged.
 *
 * After issuing a command, you shouldnt interpret the status register
 * until after a pause of this lenghth.
 */
static void ata_wait_400(void) {
    for (uint32_t i = 0; i < 14; i++) {
        inb(ATA_REG_ALT_STAT);
    }
}

/**
 * Wait 400ns, then wait until the BSY bit is 0.
 *
 * Returns the final value of the status register.
 */
static uint8_t ata_wait_complete(void) {
    ata_wait_400();

    uint8_t status;

    do {
        status = inb(ATA_REG_ALT_STAT);
    } while (status & ATA_REG_STATUS_BSY_MASK);

    return status;
}

static void load_lba(uint32_t lba) {
    outb(ATA_REG_LBA0,    lba  &       0xFF);          // 0-7
    outb(ATA_REG_LBA1,   (lba  &     0xFF00) >> 8);    // 8-15
    outb(ATA_REG_LBA2,   (lba  &   0xFF0000) >> 16);   // 16-23
    outb(ATA_REG_DEV_HD, (lba  & 0x0F000000) >> 24);   // 24-27
}

static void load_lba_and_sc(uint32_t lba, uint8_t sc) {
    outb(ATA_REG_SEC_COUNT, sc);
    load_lba(lba);
}

void ata_init(void) {
    outb(ATA_REG_DEV_HD, 0xA0); // Select Master Drive.
                    
    // Clear registers. (Chatgpt says to do this for some reason)
    load_lba_and_sc(0, 0);
}

fernos_error_t ata_read_pio(uint32_t lba, uint32_t num_sectors, void *buf) {
    // Remember, we only get 28-bits for an LBA.
    

    
}


void run_ata_test(void) {
    // Right now interrupts are disabled btw... Might want to change this later!!
    term_put_s("Hello From ata code\n");

    outb(ATA_REG_DEV_HD, 0xA0); // Select Master Drive.

    // Clear registers. (Chatgpt says to do this for some reason)
    outb(ATA_REG_SEC_COUNT, 0x0);
    outb(ATA_REG_LBA0, 0x0);
    outb(ATA_REG_LBA1, 0x0);
    outb(ATA_REG_LBA2, 0x0);

    // Ok, now it'd be cool to read and write over PIO.

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
