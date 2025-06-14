
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

/**
 * We wait for 512 Bytes to be readable or writeable over the data register.
 *
 * This starts with a 400ns wait BTW.
 */
static fernos_error_t ata_wait_data_ready(void) {
    ata_wait_400ns();

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

    return FOS_SUCCESS;
}

static void load_lba(uint32_t lba) {
    outb(ATA_REG_LBA0,    lba  &       0xFF);          // 0-7
    outb(ATA_REG_LBA1,   (lba  &     0xFF00) >> 8);    // 8-15
    outb(ATA_REG_LBA2,   (lba  &   0xFF0000) >> 16);   // 16-23
    outb(ATA_REG_DEV_HD,((lba  & 0x0F000000) >> 24) | 0xE0);   // 24-27
}

static void load_lba_and_sc(uint32_t lba, uint8_t sc) {
    outb(ATA_REG_SEC_COUNT, sc);
    load_lba(lba);
}

void ata_init(void) {
    outb(ATA_REG_DEV_HD, 0xE0); // Select Master Drive.
                    
    // Clear registers. (Chatgpt says to do this for some reason)
    load_lba_and_sc(0, 0);

    // We will always start in polling mode by default.
    ata_disable_intr();

    // Be safe since we switched the drive.
    ata_wait_400ns();
}

fernos_error_t ata_identify(uint16_t *id_buf) {
    outb(ATA_REG_COMMAND, 0xEC); // Identify command.

    fernos_error_t err = ata_wait_data_ready();
    if (err != FOS_SUCCESS) {
        return err;
    }

    for (uint32_t w = 0; w < 256; w++) {
        id_buf[w] = inw(ATA_REG_DATA);
    }

    ata_wait_400ns();

    return FOS_SUCCESS;
}

fernos_error_t ata_num_sectors(uint32_t *ns) {
    uint16_t id_buf[256];

    fernos_error_t err = ata_identify(id_buf);
    if (err != FOS_SUCCESS) {
        return err;
    }
    
    uint32_t num_sectors = 
        (uint32_t)(id_buf[61] << 16) | 
        (uint32_t)(id_buf[60]);

    *ns = num_sectors;

    return FOS_SUCCESS;
}

fernos_error_t ata_rw_pio(bool read, uint32_t lba, uint8_t sc, void *buf) {
    load_lba_and_sc(lba, sc); 

    outb(ATA_REG_COMMAND, read ? 0x20 : 0x30); // READ or WRITE SECTORS command.

    uint32_t true_sc = sc == 0 ? 256 : sc;

    uint16_t *wbuf = (uint16_t *)buf;

    for (uint32_t s = 0; s < true_sc; s++) {
        fernos_error_t err = ata_wait_data_ready();
        if (err != FOS_SUCCESS) {
            return err;
        }

        // 256 words in a sector.
        for (uint32_t w = 0; w < 256; w++) {
            if (read) {
                wbuf[(s * 256) + w] = inw(ATA_REG_DATA);
            } else {
                outw(ATA_REG_DATA, wbuf[(s * 256) + w]);
            }
        }
    }

    // Unsure if this is really needed, but waiitng at the end after the final
    // read/write just to be safe.
    ata_wait_400ns();

    return FOS_SUCCESS;
}

void run_ata_test(void) {
    // Right now interrupts are disabled btw... Might want to change this later!!
    term_put_s("Hello From ata code\n");

    fernos_error_t err;

    ata_init();

    uint32_t ns;
    err = ata_num_sectors(&ns);

    if (err != FOS_SUCCESS) {
        term_put_s("AHHH\n");
    } else {
        term_put_fmt_s("Num Sectors: %u\n", ns);
    }

    uint16_t wbuf[256];

    for (uint32_t i = 1; i < ns; i++) {
        err = ata_read_pio(i, 1, wbuf);
        if (err != FOS_SUCCESS) {
            term_put_fmt_s("Bad Read LBA: %u\n", i);
            break;
        }
    }

    term_put_s("DONE\n");

    lock_up();
}
