
#include "k_sys/ata.h"

#include "k_bios_term/term.h"
#include "k_sys/io.h"

#include <stdint.h>

void ata_wait_400ns(void) {
    for (uint32_t i = 0; i < 4; i++) {
        inb(ATA_REG_ALT_STAT);
    }
}

uint8_t ata_wait_complete(void) {
    ata_wait_400ns();

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

    // Be safe since we switched the drive.
    ata_wait_400ns();
}

fernos_error_t ata_rw_pio(bool read, uint32_t lba, uint8_t sc, void *buf) {
    load_lba_and_sc(lba, sc); 

    outb(ATA_REG_COMMAND, read ? 0x20 : 0x30); // READ or WRITE SECTORS command.
    ata_wait_400ns(); // Wait.

    uint32_t true_sc = sc == 0 ? 256 : sc;

    uint16_t *wbuf = (uint16_t *)buf;

    for (uint32_t s = 0; s < true_sc; s++) {
        uint8_t status;

        // Wait until busy mask isn't set.
        do {
            status = inb(ATA_REG_STATUS);
        } while (status & ATA_REG_STATUS_BSY_MASK);

        if (status & ATA_REG_STATUS_ERR_MASK) {
            // Error.
            return FOS_UNKNWON_ERROR;            
        }

        if (status & ATA_REG_STATUS_DF_MASK) {
            // Data Fault.
            return FOS_UNKNWON_ERROR;            
        }

        // Make sure data is actually ready. (Otherwise also an error).
        if (!(status & ATA_REG_STATUS_DRQ_MASK)) {
            // Something else wrong I guess.
            return FOS_UNKNWON_ERROR;
        }

        // 256 words in a sector.
        for (uint32_t w = 0; w < 256; w++) {
            if (read) {
                wbuf[(s * 256) + w] = inw(ATA_REG_DATA);
            } else {
                outw(ATA_REG_DATA, wbuf[(s * 256) + w]);
            }
        }

        ata_wait_400ns(); // I think I am supposed to wait after completing a sector.
    }

    return FOS_SUCCESS;
}

void run_ata_test(void) {
    // Right now interrupts are disabled btw... Might want to change this later!!
    term_put_s("Hello From ata code\n");

    ata_init();
    ata_disable_intr();

    fernos_error_t err;

    uint16_t wbuf[512];
    uint16_t rbuf[512];

    for (uint16_t i = 0; i < 512; i++) {
        wbuf[i] = i;
    }

    err = ata_write_pio(100, 2, wbuf);
    if (err != FOS_SUCCESS) {
        term_put_s("PIO Write Failed\n");
    }

    err = ata_read_pio(101, 1, rbuf);
    if (err != FOS_SUCCESS) {
        term_put_s("PIO Read Failed\n");
    }

    for (uint32_t i = 0; i < 10; i++) {
        term_put_fmt_s("0x%X\n", rbuf[i]);
    }

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


    lock_up();
}
