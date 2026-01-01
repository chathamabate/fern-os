
#include "k_startup/gfx.h"

#include "os_defs.h"
#include "s_util/constraints.h"
#include "k_sys/debug.h"
#include "s_util/str.h"
#include "s_gfx/mono_fonts.h"

void gfx_to_screen(gfx_screen_t *screen, gfx_buffer_t *frame) {
    // We'll put a memory barrier both before and after cause why not.
    __sync_synchronize();

    int32_t rendered_width = MIN(screen->width, frame->width);
    int32_t rendered_height = MIN(screen->height, frame->height);

    for (int32_t r = 0; r < rendered_height; r++) {
        volatile gfx_color_t *screen_row = (volatile gfx_color_t *)((volatile uint8_t *)(screen->buffer) + (screen->pitch * r));
        gfx_color_t *frame_row = frame->buffer + (frame->width * r);

        // It's possible a memcpy would be safe here, but I am not taking any chances with MMIO.
        for (int32_t c = 0; c < rendered_width; c++) {
            screen_row[c] = frame_row[c]; 
        }
    }

    __sync_synchronize();
}

static gfx_screen_t screen = {
    .buffer = NULL // Not initialized at first.
};

gfx_screen_t * const SCREEN = &screen;

/**
 * Remember that `init_screen` will fail if the memory mapped buffer doesnt have
 * pixels in these dimmensions.
 */
static gfx_color_t back_buffer_arr[FERNOS_GFX_HEIGHT][FERNOS_GFX_WIDTH];

static gfx_buffer_t back_buffer = {
    .al = NULL, // Statically allocated.
    
    .width = FERNOS_GFX_WIDTH,
    .height = FERNOS_GFX_HEIGHT,
    .buffer = (gfx_color_t *)back_buffer_arr
};

gfx_buffer_t * const BACK_BUFFER = &back_buffer;

fernos_error_t init_screen(const m2_info_start_t *m2_info) {
    if (!m2_info) {
        return FOS_E_BAD_ARGS;
    }

    if (screen.buffer) {
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

    if (end < fb_tag->addr || end > 0xFFFFFFFF) {
        return FOS_E_STATE_MISMATCH; // If there was wrap OR the buffer exceeds the 32-bit memory area,
                                     // don't even try to write.
    }

    // Confirm the screen is setup as expected! 
    // (Remember, the final page of the epilogue will be unmappable in kernel space)
    if (fb_tag->addr < FOS_AREA_END || end > ALIGN(0xFFFFFFFF, M_4K) ||
            fb_tag->width != FERNOS_GFX_WIDTH ||
            fb_tag->height != FERNOS_GFX_HEIGHT || fb_tag->bpp != FERNOS_GFX_BPP) {

        // An error at this point means that a frame buffer was setup, but not in the expected
        // configuration. So, let's just write 0xFF's to the entire buffer area!

        __sync_synchronize();
        for (volatile uint8_t *ptr = (volatile uint8_t *)(uint32_t)(fb_tag->addr); 
                ptr < (uint8_t *)(uint32_t)end; ptr++) {
            *ptr = 0xFF;
        }
        __sync_synchronize();

        return FOS_E_STATE_MISMATCH;
    }

    // Now actually initialize the screen buffer struct in this file! 

    screen = (gfx_screen_t) {
        .pitch = fb_tag->pitch,
        .width = fb_tag->width,
        .height = fb_tag->height,
        .buffer = (gfx_color_t *)(uint32_t)(fb_tag->addr)
    };

    return FOS_E_SUCCESS;
}

void gfx_out_fatal(const char *msg) {
    gfx_draw_ascii_mono_text(BACK_BUFFER, 
        msg, ASCII_MONO_8X16, 0, 0, 2, 2, 
        gfx_color(255, 255, 255),
        gfx_color(255, 0, 0)
    );
    gfx_render();
    lock_up();
}
