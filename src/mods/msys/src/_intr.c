

#include <stdint.h>
static volatile uint16_t * const TERMINAL_BUFFER = (uint16_t *)0xB8000;

void _my_handler(void) {
    TERMINAL_BUFFER[0] = ('a') | 0x0F00;

}
