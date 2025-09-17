
#include "k_sys/ata.h"

#include "k_bios_term/term.h"
#include "k_sys/io.h"

#include <stdint.h>

void ata_wait_400ns(void) {
    for (uint32_t i = 0; i < 4; i++) {
        inb(ATA_REG_ALT_STAT);
    }
}

/*
 * NOTE: New functions which actually issues commands to the drive
 * should alwasy start with an `ata_wait_complete` call.
 *
 * This confirms that the drive is actually ready to accept commands!
 */

uint8_t ata_wait_complete(void) {
    ata_wait_400ns();

    uint8_t status;

    do {
        status = inb(ATA_REG_ALT_STAT);

        // Loop anther command is ready to be received.
    } while ((status & ATA_REG_STATUS_BSY_MASK) 
            || !(status & ATA_REG_STATUS_DRDY_MASK));

    return status;
}

#define ATA_WAIT_DATA_READY_RETRIES (30U)

/**
 * We wait for 512 Bytes to be readable or writeable over the data register.
 *
 * With a wait complete btw.
 */
static fernos_error_t ata_wait_data_ready(void) {
     uint8_t status = ata_wait_complete();

    // Now poll for DRQ to be set.
    for (uint32_t i = 0; i < ATA_WAIT_DATA_READY_RETRIES; i++) {
        if (status & ATA_REG_STATUS_ERR_MASK) {
            // Error.
            return FOS_E_UNKNWON_ERROR;            
        }

        if (status & ATA_REG_STATUS_DF_MASK) {
            // Data Fault.
            return FOS_E_UNKNWON_ERROR;            
        }

        // If data is ready, we can exit!
        if (status & ATA_REG_STATUS_DRQ_MASK) {
            return FOS_E_SUCCESS;
        }

        status = inb(ATA_REG_ALT_STAT);
    }

    // DRQ was never set.
    return FOS_E_UNKNWON_ERROR;
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
}

fernos_error_t ata_identify(uint16_t *id_buf) {
    ata_wait_complete();

    outb(ATA_REG_COMMAND, 0xEC); // Identify command.

    fernos_error_t err = ata_wait_data_ready();
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    for (uint32_t w = 0; w < 256; w++) {
        id_buf[w] = inw(ATA_REG_DATA);
    }

    return FOS_E_SUCCESS;
}

fernos_error_t ata_num_sectors(uint32_t *ns) {
    uint16_t id_buf[256];

    fernos_error_t err = ata_identify(id_buf);
    if (err != FOS_E_SUCCESS) {
        return err;
    }
    
    uint32_t num_sectors = 
        (uint32_t)(id_buf[61] << 16) | 
        (uint32_t)(id_buf[60]);

    *ns = num_sectors;

    return FOS_E_SUCCESS;
}

fernos_error_t ata_rw_pio(bool read, uint32_t lba, uint8_t sc, void *buf) {
    ata_wait_complete();

    load_lba_and_sc(lba, sc); 

    outb(ATA_REG_COMMAND, read ? 0x20 : 0x30); // READ or WRITE SECTORS command.

    uint32_t true_sc = sc == 0 ? 256 : sc;

    uint16_t *wbuf = (uint16_t *)buf;

    for (uint32_t s = 0; s < true_sc; s++) {
        fernos_error_t err = ata_wait_data_ready();
        if (err != FOS_E_SUCCESS) {
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

    return FOS_E_SUCCESS;
}
