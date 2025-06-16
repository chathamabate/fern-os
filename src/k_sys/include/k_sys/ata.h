
#pragma once

#include "s_util/err.h"
#include "k_sys/io.h"
#include <stdbool.h>

/*
 * Here are the register port values.
 * (Using only the primary bus here)
 *
 * NOTE: I have quoted the ATA spec in this header file to make my life easier.
 */

/*
 * Command Block Registers.
 */

#define ATA_BAR0 (0x1F0U)

#define ATA_REG_DATA       (ATA_BAR0 + 0) // R/W
#define ATA_REG_ERROR      (ATA_BAR0 + 1) // R
#define ATA_REG_FEATURES   (ATA_BAR0 + 1) // W

/**
 * Sector Count Register.
 *
 * "This register contains the number of sectors of data requested to be transferred on a read or 
 * write operation between the host and the device. 
 * If the value in this register is zero, a count of 256 sectors is specified."
 */
#define ATA_REG_SEC_COUNT  (ATA_BAR0 + 2) // R/W
                                          
/**
 * LB0 Register AKA Sector Number Register.
 *
 * "In LBA Mode, this register contains Bits 7-0 of the LBA for any media access. 
 * This register is used by some non-media access commands to pass command specific information 
 * from the host to the device, or from the device to the host. 
 * This register shall be updated to reflect the media address of the error when a media access 
 * command is unsuccessfully completed."
 */
#define ATA_REG_LBA0       (ATA_BAR0 + 3) // R/W
#define ATA_REG_LBA1       (ATA_BAR0 + 4) // R/W
#define ATA_REG_LBA2       (ATA_BAR0 + 5) // R/W

/**
 * ATA Device Head Register.
 *
 * "This register contains device addressing and sector addressing information."
 */
#define ATA_REG_DEV_HD     (ATA_BAR0 + 6) // R/W
                                          
/**
 * When set to 1, we are in LBA mode (Which we should always be using tbh)
 */
#define ATA_REG_DEV_HD_L_OFF (6)
#define ATA_REG_DEV_HD_L_MASK (1UL << ATA_REG_DEV_HD_L_OFF)

/**
 * Device selector. 1 for device 1, 0 for device 0.
 * Device 0 = Master
 * Device 1 = Slave (Really just secondary drive I think)
 */
#define ATA_REG_DEV_HD_DEV_OFF (4)
#define ATA_REG_DEV_HD_DEV_MASK (1UL << ATA_REG_DEV_HD_DEV_OFF)

/**
 * The bottom 4 bits of the device/head register are the highest 4 bits of the address being
 * used. (either in CHS or LBA mode)
 */
#define ATA_REG_DEV_HD_HS_MASK (0x0FUL)
                                          
/**
 * ATA Status Register.
 *
 * "This register contains the device status. 
 * The contents of this register are updated to reflect the current state of the device and the 
 * progress of any command being executed by the device. 
 * When the BSY bit is equal to zero, the other bits in this register are valid and the other 
 * Command Block register may contain meaningful information. 
 * When the BSY bit is equal to one, no other bits in this register and all other Command Block 
 * registers are not valid."
 *
 * VERY IMPORTANT!!!!
 * "host implementations should wait at least 400 ns before reading the status register to insure 
 * that the BSY bit is valid."
 */
#define ATA_REG_STATUS     (ATA_BAR0 + 7) // R

/**
 * "BSY (Busy) is set whenever the device has control of the command Block Registers. 
 * When the BSY bit is equal to one, a write to a command block register by the host shall be 
 * ignored by the device."
 *
 * There's a lot on the busy bit in the spec, so may want to check there for more details.
 */
#define ATA_REG_STATUS_BSY_OFF  (7)
#define ATA_REG_STATUS_BSY_MASK  (1UL << ATA_REG_STATUS_BSY_OFF)

/**
 * "DRDY (Device Ready) is set to indicate that the device is capable of accepting all 
 * command codes."
 */
#define ATA_REG_STATUS_DRDY_OFF (6)
#define ATA_REG_STATUS_DRDY_MASK (1UL << ATA_REG_STATUS_DRDY_OFF)

/**
 * "DF (Device Fault) indicates a device fault error has been detected."
 */
#define ATA_REG_STATUS_DF_OFF   (5)
#define ATA_REG_STATUS_DF_MASK   (1UL << ATA_REG_STATUS_DF_OFF)

/**
 * "DSC (Device Seek Complete) indicates that the device heads are settled over a track. 
 * When an error occurs, this bit shall not be changed until the Status Register is read by the 
 * host, at which time the bit again indicates the current Seek Complete status."
 */
#define ATA_REG_STATUS_DSC_OFF  (4)
#define ATA_REG_STATUS_DSC_MASK  (1UL << ATA_REG_STATUS_DSC_OFF)

/**
 * "DRQ (Data Request) indicates that the device is ready to transfer a word or byte of data between 
 * the host and the device."
 */
#define ATA_REG_STATUS_DRQ_OFF  (3)
#define ATA_REG_STATUS_DRQ_MASK  (1UL << ATA_REG_STATUS_DRQ_OFF)

/**
 * "CORR (Corrected Data) is used to indicate a correctable data error. 
 * The definition of what constitutes a correctable error is vendor specific. 
 * This condition does not terminate a data transfer."
 */
#define ATA_REG_STATUS_CORR_OFF (2)
#define ATA_REG_STATUS_CORR_MASK (1UL << ATA_REG_STATUS_CORR_OFF)

/**
 * "IDX (Index) is vendor specific."
 */
#define ATA_REG_STATUS_IDX_OFF  (1)
#define ATA_REG_STATUS_IDX_MASK  (1UL << ATA_REG_STATUS_IDX_OFF)

/**
 * "ERR (Error) indicates that an error occurred during execution of the previous command. 
 * The bits in the Error register have additional information regarding the cause of the error."
 */
#define ATA_REG_STATUS_ERR_OFF  (0)
#define ATA_REG_STATUS_ERR_MASK  (1UL << ATA_REG_STATUS_ERR_OFF)

/**
 * "This register contains the command code being sent to the device. 
 * Command execution begins immediately after this register is written."
 */
#define ATA_REG_COMMAND    (ATA_BAR0 + 7) // W

/*
 * Control Block Registers.
 */

#define ATA_BAR1 (0x3F0U)

/**
 * Alternate Status Register.
 *
 * "This register contains the same information as the Status register in the command block. 
 * The only difference being that reading this register does not imply interrupt acknowledge or 
 * clear a pending interrupt."
 */
#define ATA_REG_ALT_STAT   (ATA_BAR1 + 6) // R

/**
 * Device Control Register.
 */
#define ATA_REG_DEV_CTL    (ATA_BAR1 + 6) // W

/**
 * "SRST is the host software reset bit. 
 * The device is held reset when this bit is set. If two devices are daisy
 * chained on the interface, this bit resets both simultaneously;"
 */
#define ATA_REG_DEV_CTL_SRST_OFF (2)
#define ATA_REG_DEV_CTL_SRST_MASK (1UL << ATA_REG_DEV_CTL_SRST_OFF)

/**
 * "nIEN is the enable bit for the device interrupt to the host. 
 * When the nIEN bit is equal to zero, and the device is selected, INTRQ shall be enabled through 
 * a tri-state buffer. 
 * When the nIEN bit is equal to one, or the device is not selected, the INTRQ signal shall be in 
 * a high impedance state."
 */
#define ATA_REG_DEV_CTL_IEN_OFF (1)
#define ATA_REG_DEV_CTL_IEN_MASK (1UL << ATA_REG_DEV_CTL_IEN_OFF)


/*
 * Simple Driver Functions.
 */

/**
 * Size of a sector in bytes.
 */
#define ATA_SECTOR_SIZE (512U)

/*
 * NOTE: The below driver is quite simple, it assumes 28bit LBA.
 */

/**
 * This should wait around 400ns which is the recommended pause to wait
 * before assuming your command has been acknowledged.
 *
 * After issuing a command, you shouldnt interpret the status register
 * until after a pause of this lenghth.
 */
void ata_wait_400ns(void);

/**
 * Wait 400ns, then wait until the BSY bit is 0 and the DRDY bit is 1.
 *
 * Returns the final value of the status register.
 */
uint8_t ata_wait_complete(void);

/**
 * Selects primary drive and disable interrupts by default.
 */
void ata_init(void);

static inline void ata_enable_intr(void) {
    ata_wait_complete(); // Unsure if needed, but doing it just to be safe!
    outb(ATA_REG_DEV_CTL, ATA_REG_DEV_CTL_IEN_MASK); 
}

static inline void ata_disable_intr(void) {
    ata_wait_complete();
    outb(ATA_REG_DEV_CTL, 0U); 
}

/**
 * Exectues the ATA IDENTIFY command and stores all words in the given ID Buf.
 * The ID Buf MUST be at least 256 Words in length.
 */
fernos_error_t ata_identify(uint16_t *id_buf);

/**
 * Retrieves the number of addressable sectors on the primary drive.
 */
fernos_error_t ata_num_sectors(uint32_t *ns);

/**
 * Read or write from Disk using PIO.
 *
 * buf should have size of at least sc * 512.
 *
 * When sc == 0, 256 sectors are read/written.
 *
 * VERY IMPORTANT: Assumes ATA interrupts are disabled!
 */
fernos_error_t ata_rw_pio(bool read, uint32_t lba, uint8_t sc, void *buf);

static inline fernos_error_t ata_read_pio(uint32_t lba, uint8_t sc, void *buf) {
    return ata_rw_pio(true, lba, sc, buf);
}

static inline fernos_error_t ata_write_pio(uint32_t lba, uint8_t sc, const void *buf) {
    return ata_rw_pio(false, lba, sc, (void *)buf);
}
