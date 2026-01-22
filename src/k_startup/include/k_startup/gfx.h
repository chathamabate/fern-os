#pragma once

#include "s_util/err.h"

#include "k_sys/m2.h"
#include "s_gfx/gfx.h"
#include "s_data/term_buffer.h"

/**
 * This structure is very similar to the generic `gfx_buffer_t` structure.
 * It is meant to reference an area of memory mapped I/O responsible for a graphics display.
 *
 * The intention is not to draw indiviual elements directly to the area one at a time, but to 
 * rather, copy a `gfx_buffer_t` over the entire area every frame.
 */
typedef struct _gfx_screen_t {
    /**
     * Bytes per row in the buffer! (must be non-negative)
     */
    int32_t pitch;

    /**
     * Visible pixels per row. (must be non-negative)
     */
    int32_t width;

    /**
     * Number of rows. (must be non-negative)
     */
    int32_t height;

    /**
     * The buffer itself with size `pitch * height` in bytes.
     */
    volatile gfx_color_t *buffer;
} gfx_screen_t;

/**
 * `frame` will be copied onto the screen anchored at the top left corner.
 * (i.e. pixel (0, 0) from the frame will be placed at pixel (0, 0) on the screen)
 *
 * If `frame` is too large, excess pixels will be ignored.
 * If `frame` is too small, pixels uncovered by `frame` will be left untouched in `screen` 
 */
void gfx_to_screen(gfx_screen_t *screen, gfx_buffer_t *frame);

/**
 * The screen as memory mapped by GRUB.
 */
extern gfx_screen_t * const SCREEN;

/**
 * Here is a statically setup buffer. 
 */
extern gfx_buffer_t * const BACK_BUFFER;

/**
 * Here is a terminal with some easy hooks for writing text directly to the screen at any time.
 *
 * This is helpful for displaying text output before entering user mode.
 * Also for printing out information during a fatal kernel error.
 *
 */
extern term_buffer_t * const DIRECT_TERM;

/**
 * This function extracts necessary information from the multiboot2 info
 * tags for setting up the screen.
 *
 * It may return an error if the framebuffer set up by grub doesn't 
 * match what was requested by the multiboot2 header tags.
 *
 * Upon success, the kernel can safely use the `SCREEN`, `BACK_BUFFER`, and `EARLY_TERM` fields
 * above!
 *
 * NOTE: If a framebuffer was setup by GRUB, but in an incorrect configuration,
 * this function will both return an error AND set the entire frame buffer to 0xFF's.
 * (So, if the screen turns full white during kernel set up, you know this function failed!)
 *
 * VERY IMPORTANT: Call this function BEFORE virtual address mapping is enabled.
 * On error, it writes to the frame buffer, wherever it may be.
 * (We assume we have access to the full memory space)
 */
fernos_error_t init_screen(const m2_info_start_t *m2_info);

/**
 * Copy the default back buffer to the default screen!
 */
static inline void gfx_render(void) {
    gfx_to_screen(SCREEN, BACK_BUFFER);
}

/**
 * Render the direct terminal to the Back buffer, and then to the screen.
 */
void gfx_direct_term_render(void);

/**
 * Put a string to the direct terminal with no formatting.
 * (Rerenders entire terminal)
 */
void gfx_direct_put_s(const char *s);

/**
 * Print a message out to the direct terminal and re render it.
 * (This rerenders the entire direct terminal every print! Very slow)
 * Making this a macro so I don't need to deal with weird varargs forwarding.
 */
#define gfx_direct_put_fmt_s(...) \
    do { \
        tb_put_fmt_s(DIRECT_TERM, __VA_ARGS__); \
        gfx_direct_term_render(); \
    } while (0)


/**
 * This function is meant to replace the old `out_bios_vga` function.
 *
 * It renders a string message onto the back buffer, renders the back buffer, than locks up the
 * CPU.
 *
 * You cannot call this function until after `init_screen` above.
 * It's purpose if for printing out a message during an extremely catostrophic error situation.
 */
void gfx_direct_fatal(const char *msg);

/*
 * The below functions I have updated to this new gfx terminal
 * design. They were used very early on in this project for
 * low-level debugging.
 */

#include "k_sys/dt.h"
#include "k_sys/idt.h"
#include "k_sys/gdt.h"
#include "k_sys/page.h"

void gfx_direct_trace(uint32_t slots, uint32_t *esp);

void gfx_direct_seg_selector(seg_selector_t ssr);
void gfx_direct_seg_desc(seg_desc_t sd);
void gfx_direct_gate_desc(gate_desc_t gd);
void gfx_direct_dtr(dtr_val_t dtv);
void gfx_direct_pte(pt_entry_t pte);
