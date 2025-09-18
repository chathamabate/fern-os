
#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * PS/2 Keyboard Scancode Set 1
 *
 * ChatGPT gave me this, hope it's all correct.
 * Can't believe there was just an easy header file online for this, but whatever.
 */

#define SCS1_ESC          0x01
#define SCS1_1            0x02
#define SCS1_2            0x03
#define SCS1_3            0x04
#define SCS1_4            0x05
#define SCS1_5            0x06
#define SCS1_6            0x07
#define SCS1_7            0x08
#define SCS1_8            0x09
#define SCS1_9            0x0A
#define SCS1_0            0x0B
#define SCS1_MINUS        0x0C  // -
#define SCS1_EQUALS       0x0D  // =
#define SCS1_BACKSPACE    0x0E
#define SCS1_TAB          0x0F
#define SCS1_Q            0x10
#define SCS1_W            0x11
#define SCS1_E            0x12
#define SCS1_R            0x13
#define SCS1_T            0x14
#define SCS1_Y            0x15
#define SCS1_U            0x16
#define SCS1_I            0x17
#define SCS1_O            0x18
#define SCS1_P            0x19
#define SCS1_LBRACKET     0x1A  // [
#define SCS1_RBRACKET     0x1B  // ]
#define SCS1_ENTER        0x1C
#define SCS1_LCTRL        0x1D
#define SCS1_A            0x1E
#define SCS1_S            0x1F
#define SCS1_D            0x20
#define SCS1_F            0x21
#define SCS1_G            0x22
#define SCS1_H            0x23
#define SCS1_J            0x24
#define SCS1_K            0x25
#define SCS1_L            0x26
#define SCS1_SEMICOLON    0x27  // ;
#define SCS1_APOSTROPHE   0x28  // '
#define SCS1_GRAVE        0x29  // `
#define SCS1_LSHIFT       0x2A
#define SCS1_BACKSLASH    0x2B  // '\'
#define SCS1_Z            0x2C
#define SCS1_X            0x2D
#define SCS1_C            0x2E
#define SCS1_V            0x2F
#define SCS1_B            0x30
#define SCS1_N            0x31
#define SCS1_M            0x32
#define SCS1_COMMA        0x33  // ,
#define SCS1_PERIOD       0x34  // .
#define SCS1_SLASH        0x35  // /
#define SCS1_RSHIFT       0x36
#define SCS1_KP_ASTERISK  0x37  // Keypad *
#define SCS1_LALT         0x38
#define SCS1_SPACE        0x39
#define SCS1_CAPSLOCK     0x3A
#define SCS1_F1           0x3B
#define SCS1_F2           0x3C
#define SCS1_F3           0x3D
#define SCS1_F4           0x3E
#define SCS1_F5           0x3F
#define SCS1_F6           0x40
#define SCS1_F7           0x41
#define SCS1_F8           0x42
#define SCS1_F9           0x43
#define SCS1_F10          0x44
#define SCS1_NUMLOCK      0x45
#define SCS1_SCROLLLOCK   0x46
#define SCS1_KP_7         0x47
#define SCS1_KP_8         0x48
#define SCS1_KP_9         0x49
#define SCS1_KP_MINUS     0x4A
#define SCS1_KP_4         0x4B
#define SCS1_KP_5         0x4C
#define SCS1_KP_6         0x4D
#define SCS1_KP_PLUS      0x4E
#define SCS1_KP_1         0x4F
#define SCS1_KP_2         0x50
#define SCS1_KP_3         0x51
#define SCS1_KP_0         0x52
#define SCS1_KP_PERIOD    0x53

/* 
 * Extended scan codes (0xE0 prefix)
 * These all require 2 bytes. (The first being the prefix)
 */

#define SCSI_EXTEND_PREFIX  0xE0

#define SCS1_E_KP_ENTER     0x1C
#define SCS1_E_RCTRL        0x1D
#define SCS1_E_KP_SLASH     0x35
#define SCS1_E_RALT         0x38
#define SCS1_E_HOME         0x47
#define SCS1_E_UP           0x48
#define SCS1_E_PAGEUP       0x49
#define SCS1_E_LEFT         0x4B
#define SCS1_E_RIGHT        0x4D
#define SCS1_E_END          0x4F
#define SCS1_E_DOWN         0x50
#define SCS1_E_PAGEDOWN     0x51
#define SCS1_E_INSERT       0x52
#define SCS1_E_DELETE       0x53
#define SCS1_E_LGUI         0x5B // Windows key
#define SCS1_E_RGUI         0x5C
#define SCS1_E_APPS         0x5D

/*
 * A "Make" scan code is when a key is pressed.
 * It's Msb is always 0. 
 *
 * A "Break" scan code is when a key is released.
 * It's Msb is always 1.
 *
 * This does not affect the prefix of extended key codes!
 * The prefix is always 0xE0, The bit change applies to the scancode byte which
 * comes after the prefix.
 */

/**
 * Use this map to get the lowercase ASCII equiv of a set 1 scancode.
 *
 * If a key has no ascii equivelant, 0 is mapped.
 * (Don't use extended scan codes in this map)
 */
const char SCS1_TO_ASCII_LC[256];

/**
 * Use this map to get the uppercase ASCII equiv of a set 1 scancode.
 *
 * If a key has no ascii equivelant, 0 is mapped.
 * (Don't use extended scan codes in this map)
 */
const char SCS1_TO_ASCII_UC[256];

/**
 * Set a scan code as "Make".
 */
static inline uint8_t scs1_as_make(uint8_t sc) {
    return sc & ~(1U << 7);
}

/**
 * Is a scan code a "Make"?
 */
static inline bool scs1_is_make(uint8_t sc) {
    return !(sc & (1U << 7));
}

/**
 * Set a scan code as "Break".
 */
static inline uint8_t scs1_as_break(uint8_t sc) {
    return sc | (1U << 7);
}

/**
 * Is a scan code a "Break"?
 */
static inline bool scs1_is_break(uint8_t sc) {
    return sc & (1U << 7);
}
