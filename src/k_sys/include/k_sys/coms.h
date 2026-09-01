
#pragma once

#include "s_util/err.h"
#include "k_sys/io.h"
#include <stdbool.h>

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
/* More com ports may exist depending on the system */

/**
 * Read from RX.
 * Write to TX.
 *
 * When DLAB=1:
 * R/W least significant byte of buad rate divisor value.
 */
#define COMS_RX_TX_OFF 0x0

/**
 * R/W Interrupt Enable Register.
 *
 * When DLAB=1:
 * R/W most significant byte of buad rate divisor value.
 */
#define COMS_INTR_EN_OFF 0x1
#define COMS_INTR_EN_RX_AVAIL_BIT (1 << 0)
#define COMS_INTR_EN_TXH_REG_EMPTY_BIT (1 << 1)
#define COMS_INTR_EN_RX_LINE_STATUS_BIT (1 << 2)
#define COMS_INTR_EN_MODEM_STATUS_BIT (1 << 3)


/**
 * Read for Interrupt Identification.
 * Write to Fifo control registers.
 */
#define COMS_FIFO_CTL_OFF 0x2

/**
 * R/w Line control register.
 */
#define COMS_LINE_CTL_OFF 0x3
#define COMS_LINE_CTL_DATA_BITS     (1 << 0) // Bits [0:1]
#define COMS_LINE_CTL_STOP_BIT      (1 << 2)
#define COMS_LINE_CTL_PARITY_BITS   (1 << 3) // Bits [3:5]
#define COMS_LINE_CTL_BRK_EN_BIT    (1 << 6)
#define COMS_LINE_CTL_DLB_BIT       (1 << 7)

/**
 * R/W Modem control register.
 */
#define COMS_MODEM_CTL_OFF 0x4

/**
 * Read Line status register.
 */
#define COMS_LINE_STATUS_OFF 0x5

/**
 * Read modem status register.
 */
#define COMS_MODEM_STATUS_OFF 0x6

/**
 * R/W Coms scratch register.
 */
#define COMS_SCRATCH_OFF 0x7

