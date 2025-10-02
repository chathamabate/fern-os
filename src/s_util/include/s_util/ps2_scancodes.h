
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "s_util/err.h"

/**
 * Scancode Set 1 code!
 *
 * For normal scancodes, the top 8 bits will always be 0.
 * For extended scancodes, the top 8 bits will be the extension prefix 0xE0.
 */
typedef uint16_t scs1_code_t;

/*
 * PS/2 Keyboard Scancode Set 1
 *
 * ChatGPT gave me this, hope it's all correct.
 * Can't believe there was just an easy header file online for this, but whatever.
 *
 * The codes below are all the "Make" codes by the way.
 */

#define SCS1_ESC          0x0001U
#define SCS1_1            0x0002U
#define SCS1_2            0x0003U
#define SCS1_3            0x0004U
#define SCS1_4            0x0005U
#define SCS1_5            0x0006U
#define SCS1_6            0x0007U
#define SCS1_7            0x0008U
#define SCS1_8            0x0009U
#define SCS1_9            0x000AU
#define SCS1_0            0x000BU
#define SCS1_MINUS        0x000CU  // -
#define SCS1_EQUALS       0x000DU  // =
#define SCS1_BACKSPACE    0x000EU
#define SCS1_TAB          0x000FU
#define SCS1_Q            0x0010U
#define SCS1_W            0x0011U
#define SCS1_E            0x0012U
#define SCS1_R            0x0013U
#define SCS1_T            0x0014U
#define SCS1_Y            0x0015U
#define SCS1_U            0x0016U
#define SCS1_I            0x0017U
#define SCS1_O            0x0018U
#define SCS1_P            0x0019U
#define SCS1_LBRACKET     0x001AU  // [
#define SCS1_RBRACKET     0x001BU  // ]
#define SCS1_ENTER        0x001CU
#define SCS1_LCTRL        0x001DU
#define SCS1_A            0x001EU
#define SCS1_S            0x001FU
#define SCS1_D            0x0020U
#define SCS1_F            0x0021U
#define SCS1_G            0x0022U
#define SCS1_H            0x0023U
#define SCS1_J            0x0024U
#define SCS1_K            0x0025U
#define SCS1_L            0x0026U
#define SCS1_SEMICOLON    0x0027U  // ;
#define SCS1_APOSTROPHE   0x0028U  // '
#define SCS1_GRAVE        0x0029U  // `
#define SCS1_LSHIFT       0x002AU
#define SCS1_BACKSLASH    0x002BU  // '\'
#define SCS1_Z            0x002CU
#define SCS1_X            0x002DU
#define SCS1_C            0x002EU
#define SCS1_V            0x002FU
#define SCS1_B            0x0030U
#define SCS1_N            0x0031U
#define SCS1_M            0x0032U
#define SCS1_COMMA        0x0033U  // ,
#define SCS1_PERIOD       0x0034U  // .
#define SCS1_SLASH        0x0035U  // /
#define SCS1_RSHIFT       0x0036U
#define SCS1_KP_ASTERISK  0x0037U  // Keypad *
#define SCS1_LALT         0x0038U
#define SCS1_SPACE        0x0039U
#define SCS1_CAPSLOCK     0x003AU
#define SCS1_F1           0x003BU
#define SCS1_F2           0x003CU
#define SCS1_F3           0x003DU
#define SCS1_F4           0x003EU
#define SCS1_F5           0x003FU
#define SCS1_F6           0x0040U
#define SCS1_F7           0x0041U
#define SCS1_F8           0x0042U
#define SCS1_F9           0x0043U
#define SCS1_F10          0x0044U
#define SCS1_NUMLOCK      0x0045U
#define SCS1_SCROLLLOCK   0x0046U
#define SCS1_KP_7         0x0047U
#define SCS1_KP_8         0x0048U
#define SCS1_KP_9         0x0049U
#define SCS1_KP_MINUS     0x004AU
#define SCS1_KP_4         0x004BU
#define SCS1_KP_5         0x004CU
#define SCS1_KP_6         0x004DU
#define SCS1_KP_PLUS      0x004EU
#define SCS1_KP_1         0x004FU
#define SCS1_KP_2         0x0050U
#define SCS1_KP_3         0x0051U
#define SCS1_KP_0         0x0052U
#define SCS1_KP_PERIOD    0x0053U

/* 
 * Extended scan codes (0xE0 prefix)
 * These all require 2 bytes. (The first being the prefix)
 */

#define SCS1_EXTEND_PREFIX  0xE0

#define SCS1_E_KP_ENTER     0xE01CU
#define SCS1_E_RCTRL        0xE01DU
#define SCS1_E_KP_SLASH     0xE035U
#define SCS1_E_RALT         0xE038U
#define SCS1_E_HOME         0xE047U
#define SCS1_E_UP           0xE048U
#define SCS1_E_PAGEUP       0xE049U
#define SCS1_E_LEFT         0xE04BU
#define SCS1_E_RIGHT        0xE04DU
#define SCS1_E_END          0xE04FU
#define SCS1_E_DOWN         0xE050U
#define SCS1_E_PAGEDOWN     0xE051U
#define SCS1_E_INSERT       0xE052U
#define SCS1_E_DELETE       0xE053U
#define SCS1_E_LGUI         0xE05BU
#define SCS1_E_RGUI         0xE05CU
#define SCS1_E_APPS         0xE05DU

/**
 * Is a scan code valid?
 */
static inline bool scs1_is_valid(scs1_code_t sc) {
    return (SCS1_ESC <= sc && sc <= SCS1_KP_PERIOD) || (SCS1_E_KP_ENTER <= sc && sc <= SCS1_E_APPS);
}

/**
 * Use this map to get the lowercase ASCII equiv of a set 1 scancode.
 *
 * If a key has no ascii equivelant, 0 is mapped.
 * (Don't use extended scan codes in this map)
 */
extern const char SCS1_TO_ASCII_LC[256];

/**
 * Get the lowercase ascii equivelant of a scancode.
 *
 * If no such character exists, 0 is returned!
 */
static inline char scs1_to_ascii_lc(scs1_code_t sc) {
    if ((sc & 0xFF00) == 0) {
        return SCS1_TO_ASCII_LC[sc];
    }
    return 0;
}

/**
 * Use this map to get the uppercase ASCII equiv of a set 1 scancode.
 *
 * If a key has no ascii equivelant, 0 is mapped.
 * (Don't use extended scan codes in this map)
 */
extern const char SCS1_TO_ASCII_UC[256];

/**
 * Get the uppercase ascii equivelant of a scancode.
 *
 * If no such character exists, 0 is returned!
 */
static inline char scs1_to_ascii_uc(scs1_code_t sc) {
    if ((sc & 0xFF00) == 0) {
        return SCS1_TO_ASCII_UC[sc];
    }
    return 0;
}


/*
 * A "Make" scan code is when a key is pressed.
 * Bit 7 always 0. 
 *
 * A "Break" scan code is when a key is released.
 * Bit 7 always 1. 
 */

/**
 * Get the uppercase ascii equivelant of a scancode.
 *
 * If no such character exists, 0 is returned!
 */
static inline scs1_code_t scs1_as_make(scs1_code_t sc) {
    return sc & ~(1U << 7);
}

/**
 * Is a scan code a "Make"?
 */
static inline bool scs1_is_make(scs1_code_t sc) {
    return !(sc & (1U << 7));
}

/**
 * Set a scan code as "Break".
 */
static inline scs1_code_t scs1_as_break(scs1_code_t sc) {
    return sc | (1U << 7);
}

/**
 * Is a scan code a "Break"?
 */
static inline bool scs1_is_break(scs1_code_t sc) {
    return sc & (1U << 7);
}

/*
 * Scancode Set 1 Listener.
 */

typedef struct _scs1_listener_t scs1_listener_t; 
typedef struct _scs1_listener_impl_t scs1_listener_impl_t;

struct _scs1_listener_impl_t {
    void (*delete_scs1_listener)(scs1_listener_t *scs1_l);

    fernos_error_t (*scs1_l_accept)(scs1_listener_t *scs1_l, scs1_code_t sc);
    void (*scs1_l_on_focus_change)(scs1_listener_t *scs1_l, bool focus);
};

/**
 * A Scan code listener is just some abstract thing which accepts scan codes.
 *
 * These will be useful in the key event interrupt handler!
 */
struct _scs1_listener_t {
    const scs1_listener_impl_t * const impl;

    /**
     * Whether or not this listener is accepting scan codes.
     */
    bool focused;
};

static inline void delete_scs1_listener(scs1_listener_t *scs1_l) {
    if (scs1_l) {
        scs1_l->impl->delete_scs1_listener(scs1_l);
    }
}

/**
 * Accept a valid Scancode Set 1 code. 
 *
 * Returns FOS_SUCCESS and does nothing if the listener is unfocused.
 * Otherwise, an error code is propegated from the implementation.
 */
static inline fernos_error_t scs1_l_accept(scs1_listener_t *scs1_l, scs1_code_t sc) {
    if (scs1_l->focused) {
        return scs1_l->impl->scs1_l_accept(scs1_l, sc);
    }
    return FOS_E_SUCCESS;
}

/**
 * Set the focus of the listener.
 *
 * Only calls the on_focus_change event if the focus value actually changes!
 */
static inline void scs1_l_set_focus(scs1_listener_t *scs1_l, bool focused) {
    if (focused != scs1_l->focused) {
        scs1_l->focused = focused;
        scs1_l->impl->scs1_l_on_focus_change(scs1_l, focused);
    }
}

static inline bool scs1_get_focus(scs1_listener_t *scs1_l) {
    return scs1_l->focused;
}

static inline void scs1_l_toggle_focus(scs1_listener_t *scs1_l) {
    scs1_l_set_focus(scs1_l, !(scs1_l->focused));
}
