
#include "k_sys/debug.h"

void out_bios_vga(uint8_t style, const char *str) {
    uint32_t i = 0;
    char c;

    while ((c = str[i]) != '\0') {
       TERMINAL_BUFFER[i] = vga_entry(c, style); 
       i++; 
    }
}
