
#include "s_gfx/test/mono_fonts.h"

void gfx_test_moving_mono_text(gfx_buffer_t *buffer, const gfx_box_t *clip_area, 
        const ascii_mono_font_t *font) {
    // We want a buffer with all the first 128 ascii characters.
    char msg[129];
    for (size_t i = 0; i < sizeof(msg); i++) {
        msg[i] = (char)i;
    }
    msg[128] = '\0';

    const uint8_t w_scale = 2;
    const uint8_t h_scale = 3;

    const uint16_t msg_width = (127 * font->char_width * w_scale);

    static int32_t x_pos = 0;
    static int32_t x_per_frame = 5;

    x_pos += x_per_frame;
    if (x_pos >= buffer->width) {
        x_pos = buffer->width;
        x_per_frame *= -1;
    } else if (x_pos <= -(int32_t)msg_width) {
        x_pos = -(int32_t)msg_width;
        x_per_frame *= -1;
    }

    gfx_draw_ascii_mono_text(buffer, clip_area, msg + 1, font, x_pos, 100, 
            w_scale, h_scale, gfx_color(0, 0, 255), gfx_color(0, 255, 0));
}
