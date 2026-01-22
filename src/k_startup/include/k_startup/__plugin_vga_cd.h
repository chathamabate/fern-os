
#pragma once

#include "plugin.h"

// TO BE DELETED/REFACTORED!

/**
 * The max number of characters which can be written to the vga terminal
 * display from userspace in one go.
 */
#define PLG_VGA_CD_TX_MAX_LEN (2048U)

plugin_t *new_plugin_vga_cd(kernel_state_t *ks);
