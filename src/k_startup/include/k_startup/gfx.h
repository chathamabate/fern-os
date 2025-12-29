#pragma once

#include "s_util/err.h"

#include "k_sys/m2.h"
#include "s_gfx/gfx.h"

extern gfx_buffer_t * const SCREEN;

/**
 * This function extracts necessary information from the multiboot2 info
 * tags for setting up the screen.
 *
 * It may return an error if the framebuffer set up by grub doesn't 
 * match what was requested by the multiboot2 header tags.
 *
 * Upon success, the kernel can safely use the `SCREEN` field above.
 *
 * NOTE: If a framebuffer was setup by GRUB, but in an incorrect configuration,
 * this function will both return an error AND set the entire frame buffer to 0xFF's.
 * (So, if the screen turns full white during kernel set up, you know this function failed!)
 */
fernos_error_t init_screen(const m2_info_start_t *m2_info);


