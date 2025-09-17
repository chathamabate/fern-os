#pragma once

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

#define I8042_R_INPUT_PORT   (0x60)
#define I8042_W_OUTPUT_PORT  (0x60)

#define I8042_R_STATUS_REG_PORT (0x64)
#define I8042_W_SEND_CMD_PORT   (0x64)
