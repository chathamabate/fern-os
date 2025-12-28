
#include "k_startup/gfx.h"

#include "os_defs.h"
#include "s_util/constraints.h"
#include "s_util/str.h"

static gfx_buffer_t screen_buffer = {
    .buffer = NULL // Not Initialized at first.
};

gfx_buffer_t * const SCREEN = &screen_buffer;

static gfx_view_t screen_view = {
    .al = NULL,
    .parent_buffer = NULL,
};

gfx_view_t * const SCREEN_VIEW = &screen_view;

fernos_error_t init_screen(const m2_info_start_t *m2_info) {
    if (!m2_info) {
        return FOS_E_BAD_ARGS;
    }

    if (screen_buffer.buffer) {
        // The screen buffer has already been initialized!
        return FOS_E_STATE_MISMATCH;
    }

    // Otherwise, let's search for the

    const m2_info_tag_base_t *tags = m2_info_tag_area(m2_info);

    const m2_info_tag_framebuffer_t *fb_tag = 
        (const m2_info_tag_framebuffer_t *)m2_find_info_tag(tags, M2_ITT_FRAMEBUFFER);
    if (!fb_tag) { // Couldn't find a framebuffer definition!
        return FOS_E_STATE_MISMATCH;
    }

    // Ok, there is a framebuffer tag!    

    uint64_t end = fb_tag->addr + (fb_tag->pitch * fb_tag->height);

    // Confirm the screen is setup as expected! 
    // (Remember, the final page of the epilogue will be unmapped in kernel space)
    if (fb_tag->addr < FOS_AREA_END || end > ALIGN(0xFFFFFFFF, M_4K) ||
            fb_tag->width != FERNOS_GFX_WIDTH ||
            fb_tag->height != FERNOS_GFX_HEIGHT || fb_tag->bpp != FERNOS_GFX_BPP) {
        mem_set((void *)(fb_tag->addr), 0xFF, end - fb_tag->addr);
        return FOS_E_STATE_MISMATCH;
    }

    // Now actually initialize the screen buffer struct in this file! 

    screen_buffer = (gfx_buffer_t) {
        .pitch = fb_tag->pitch,
        .width = fb_tag->width,
        .height = fb_tag->height,
        .buffer = (gfx_color_t *)(fb_tag->addr)
    };

    screen_view = (gfx_view_t) {
        .al = NULL,
        .parent_buffer = SCREEN,
        .origin_x = 0,
        .origin_y = 0,
        .width = fb_tag->width,
        .height = fb_tag->height,
    };

    return FOS_E_SUCCESS;
}
