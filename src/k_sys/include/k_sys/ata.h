
#pragma once

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
#define ATA_REG_SEC_COUNT  (ATA_BAR0 + 2) // R/W
#define ATA_REG_LBA0       (ATA_BAR0 + 3) // R/W
#define ATA_REG_LBA1       (ATA_BAR0 + 4) // R/W
#define ATA_REG_LBA2       (ATA_BAR0 + 5) // R/W
#define ATA_REG_DEV_HD     (ATA_BAR0 + 6) // R/W
                                          
/*
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
 * "host implementations should wait at least 400 ns before reading the status register to insure 
 * that the BSY bit is valid."
 */
#define ATA_REG_STATUS     (ATA_BAR0 + 7) // R

#define ATA_REG_STATUS_BSY_OFF  (7)
#define ATA_REG_STATUS_BSY_MASK  (1UL << ATA_REG_STATUS_BSY_OFF)

#define ATA_REG_STATUS_DRDY_OFF (6)
#define ATA_REG_STATUS_DRDY_MASK (1UL << ATA_REG_STATUS_DRDY_OFF)

#define ATA_REG_STATUS_DF_OFF   (5)
#define ATA_REG_STATUS_DF_MASK   (1UL << ATA_REG_STATUS_DF_OFF)

#define ATA_REG_STATUS_DSC_OFF  (4)
#define ATA_REG_STATUS_DSC_MASK  (1UL << ATA_REG_STATUS_DSC_OFF)

#define ATA_REG_STATUS_DRQ_OFF  (3)
#define ATA_REG_STATUS_DRQ_MASK  (1UL << ATA_REG_STATUS_DRQ_OFF)

#define ATA_REG_STATUS_CORR_OFF (2)
#define ATA_REG_STATUS_CORR_MASK (1UL << ATA_REG_STATUS_CORR_OFF)

#define ATA_REG_STATUS_IDX_OFF  (1)
#define ATA_REG_STATUS_IDX_MASK  (1UL << ATA_REG_STATUS_IDX_OFF)

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

#define ATA_REG_ALT_STAT   (ATA_BAR1 + 6) // R
#define ATA_REG_DEV_CTL    (ATA_BAR1 + 6) // W


/*
 * Device Control Register Masks.
 */

/**
 * Software Reset Bit.
 */
#define DEV_CTL_SRST_OFF (2)
#define DEV_CTL_SRST_MASK (1UL << DEV_CTL_SRST_OFF)

/**
 * When 0 and device is selected, interrupts are enabled!
 */
#define DEV_CTL_IEN_OFF (1)
#define DEV_CTL_IEN_MASK (1UL << DEV_CTL_IEN_OFF)

/*
 * Device/Head Register Masks.
 */

/**
 * When set to 1, we are in LBA mode (Which we should always be using tbh)
 */
#define DEV_HD_L_OFF (6)
#define DEV_HD_L_MASK (1UL << DEV_HD_L_OFF)

/**
 * Device selector. 1 for device 1, 0 for device 0.
 */
#define DEV_HD_DEV_OFF (4)
#define DEV_HD_DEV_MASK (1UL << DEV_HD_DEV_OFF)

/**
 * The bottom 4 bits of the device/head register are the highest 4 bits of the address being
 * used. (either in CHS or LBA mode)
 */
#define DEV_HD_HS_MASK (0x0FUL)

/*
 * Leaving Error register details out for now, in case of error, just check the manual to interpret
 * the error register value.
 */

/* 
 * Some register definitions from the docs:
 *
 * Sector Count Register: 
 * This register contains the number of sectors of data requested to be transferred on a read or 
 * write operation between the host and the device. 
 * If the value in this register is zero, a count of 256 sectors is specified.
 *
 * Sector Number Register:
 * In LBA Mode, this register contains Bits 7-0 of the LBA for any media access. 
 * This register is used by some non-media access commands to pass command specific information 
 * from the host to the device, or from the device to the host. 
 * This register shall be updated to reflect the media address of the error when a media access 
 * command is unsuccessfully completed.
 */

void run_ata_test(void);
