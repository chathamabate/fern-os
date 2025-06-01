
#pragma once

/*
 * ATA Notes
 *
 *
 */

/*
 * Alt Status Register Masks.
 */

#define ALT_STAT_BSY_OFF  (7)
#define ALT_STAT_BSY_MASK  (1UL << ALT_STAT_BSY_OFF)

#define ALT_STAT_DRDY_OFF (6)
#define ALT_STAT_DRDY_MASK (1UL << ALT_STAT_DRDY_OFF)

#define ALT_STAT_DF_OFF   (5)
#define ALT_STAT_DF_MASK   (1UL << ALT_STAT_DF_OFF)

#define ALT_STAT_DSC_OFF  (4)
#define ALT_STAT_DSC_MASK  (1UL << ALT_STAT_DSC_OFF)

#define ALT_STAT_DRQ_OFF  (3)
#define ALT_STAT_DRQ_MASK  (1UL << ALT_STAT_DRQ_OFF)

#define ALT_STAT_CORR_OFF (2)
#define ALT_STAT_CORR_MASK (1UL << ALT_STAT_CORR_OFF)

#define ALT_STAT_IDX_OFF  (1)
#define ALT_STAT_IDX_MASK  (1UL << ALT_STAT_IDX_OFF)

#define ALT_STAT_ERR_OFF  (0)
#define ALT_STAT_ERR_MASK  (1UL << ALT_STAT_ERR_OFF)

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


