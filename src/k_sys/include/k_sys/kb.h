#pragma once

#include "k_sys/io.h"
#include "s_util/err.h"

#define PS2_ACK (0xFA)

/*
 * Host to keyboard commands.
 *
 * We'll just list the basic/essential ones
 *
 * NOTE: Every byte sent to the keyboard will get ACK as a response (0xFA)
 * (Except for Resend and Echo commands)
 */

/**
 * Reset the keyboard.
 */
#define PS2_CMD_RESET (0xFF)

/**
 * Resend the last scan code or response bytes.
 * (This does NOT respond with ACK)
 */
#define PS2_CMD_RESEND (0xFE)

/**
 * Return the keyboard to its default settings.
 */
#define PS2_CMD_SET_DEFAULT (0xF6)

/**
 * Disables the keyboard and returns it to its default settings.
 */
#define PS2_CMD_DISABLE (0xF5)

/**
 * Enable the keybaord after previously being disabled.
 */
#define PS2_CMD_ENABLE (0xF4)

/**
 * Set the keyboards typematic repeat rate and delay.
 */
#define PS2_CMD_SET_REPEAT_RATE_AND_DELAY (0xF3)

/*
 * After issuing the above command, one argument byte is expected.
 * This should be a REPEAT_RATE OR'd with a DELAY.
 *
 * Repeat rates are in units characters per second.
 * Delay is in units seconds.
 */

#define PS2_REPEAT_RATE_2    (0x00)
#define PS2_REPEAT_RATE_4    (0x08)
#define PS2_REPEAT_RATE_8    (0x10)
#define PS2_REPEAT_RATE_16   (0x18)

#define PS2_DELAY_0_25  (0x00)
#define PS2_DELAY_0_50  (0x20)
#define PS2_DELAY_1_00  (0x60)

/**
 * The keyboard should respond with 0xEE.
 */
#define PS2_CMD_ECHO (0xEE)

/**
 * Set the LEDs on the keyboard.
 */
#define PS2_CMD_SET_LEDS (0xED)

#define PS2_LED_SCROLL_LOCK (1U << 0)
#define PS2_LED_NUM_LOCK    (1U << 1)
#define PS2_LED_CAPS_LOCK   (1U << 2)

/*
 * We'll be communicating with the keyboard via an 8042 controller.
 */

/**
 * Read Output from the keyboard/controller.
 */
#define I8042_R_OUTPUT_PORT   (0x60)

/**
 * Write output to be sent to the keyboard/controller.
 */
#define I8042_W_INPUT_PORT  (0x60)

#define I8042_R_STATUS_REG_PORT (0x64)
#define I8042_W_SEND_CMD_PORT   (0x64)

/*
 * Status Register Bit meanings.
 */

/**
 * Output buffer full. (1 = Data in the output buffer ready to be read)
 */
#define I8042_STATUS_REG_OBF    (1U << 0)

/**
 * Input buffer full. (1 = Your input is being processed still, don't write to the input port)
 */
#define I8042_STATUS_REG_IBF    (1U << 1)

/**
 * System flag, shouldn't need to use this.
 */
#define I8042_STATUS_REG_SYS    (1U << 2)

/**
 * Address line A2, (0 = 0x60 was last written to, 1 = 0x64 was last written to)
 * Shouldn't really need to use this either.
 */
#define I8042_STATUS_REG_A2     (1U << 3)

/**
 * Inhibit flag (0 = Keyboard is inhibited, 1 = Keyboard is NOT inhibited)
 */
#define I8042_STATUS_REG_INH    (1U << 4)

/**
 * Mouse output buffer full.
 */
#define I8042_STATUS_REG_MOBF   (1U << 5)

/**
 * Loop Until output buffer is full.
 *
 * MIGHT BE DANGEROUS
 */
static inline void i8042_wait_for_full_output_buffer(void) {
    while (!(inb(I8042_R_STATUS_REG_PORT) & I8042_STATUS_REG_OBF));
}

/**
 * Loop until input buffer is empty.
 *
 * MIGHT BE DANGEROUS
 */
static inline void i8042_wait_for_empty_input_buffer(void) {
    while (inb(I8042_R_STATUS_REG_PORT) & I8042_STATUS_REG_IBF);
}

/**
 * Timeout Error
 * (0 = No error, 1 = TxTO or RxTO error)
 */
#define I8042_STATUS_REG_TO     (1U << 6)

/**
 * Communication error with keyboard.
 * (0 = No Error, 1 = Parity error or 0xFE received as command response)
 */
#define I8042_STATUS_REG_PERR   (1U << 7)

/*
 * We can send an I8042 command by writing to the send command port (0x64)
 *
 * "Command parameters are written to port 0x60 after the command is sent. 
 * Results are returned on port 0x60. Always test the OBF ("Output Buffer Full") 
 * flag before writing commands or parameters to the 8042." - Adam Chapweske
 *
 * Unsure why above it says I need to check OBF before writing commands to the I8042.
 * Maybe the I8042 only returns a value if the output register is empty?
 *
 * But anyway, writing to the Command register is kinda confusing as no ACK byte
 * is ever received. So I guess just be careful? This should only be dealt with
 * at init time, I doubt I will be sending I8042 commands during any other time.
 */

#define I8042_CMD_READ_CMD_BYTE  (0x20)
#define I8042_CMD_WRITE_CMD_BYTE (0x60)

/**
 * Test the I8042 is working correctly, should return 0x55.
 */
#define I8042_CMD_CONTROLLER_SELF_TEST (0xAA)

/**
 * Self test the keyboard interface, should return 0x00.
 */
#define I8042_CMD_KB_IF_SELF_TEST (0xAB)

/**
 * Disable keyboard interface.
 */
#define I8042_CMD_DISABLE_KB_IF (0xAD)

/**
 * Enable keyboard interface.
 */
#define I8042_CMD_ENABLE_KB_IF (0xAE)

/**
 * When set, the controller will send IRQ1 when data is available in the input buffer.
 * (Otherwise you must poll)
 */
#define I8042_CMDBYTE_INT (1U << 0)

/**
 * When set, Scan codes are translated to set 1.
 * Otherwise, they are not translated at all.
 */
#define I8042_CMDBYTE_XLAT (1U << 6)

/**
 * If this call is successful, the keyboard should start sending 
 * interrupts with scancodes! (Should call this way before interrupts are enabled)
 */
fernos_error_t init_kb(void);

