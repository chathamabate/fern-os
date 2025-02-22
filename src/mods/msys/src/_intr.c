

#include <stdint.h>

static volatile uint16_t * const TERMINAL_BUFFER = (uint16_t *)0xB8000;

// Bare Bones Print. 
static inline uint32_t put_msg(const char *msg) {
    uint32_t i = 0;

    while (msg[i]) {
        TERMINAL_BUFFER[i] = msg[i] | 0x0F00; 
        i++;
    }

    return i;
}

void _default_exception_handler(void) {
    put_msg("Default exception handler called, locking up!");
}
void _default_exception_with_err_code_handler(uint32_t err) {

}
void _default_handler(void) {
    put_msg("Default handler called, returning.");
}
