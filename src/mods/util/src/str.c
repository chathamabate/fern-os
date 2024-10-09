
#include "util/str.h"

#include "msys/test.h"

bool str_eq(const char *s1, const char *s2) {
    size_t i = 0;
    while (true) {
        if (s1[i] != s2[i]) {
            return false;
        }

        if (s1[i] == '\0') {
            return true;
        }

        i++;
    }
}

size_t str_of_u(char *buf, uint32_t u) {
    if (u == 0) {
        buf[0] = '0';
        buf[1] = '\0';

        return 1;
    }

    char temp[10];

    size_t i = 0;   // Number of digits found.
    uint32_t val = u;
    while (val > 0) {
        temp[i++] = '0' + (val % 10);
        val /= 10;
    }

    // Copy temp over to the buffer in the correct order.
    for (size_t j = 0; j < i; j++) {
        buf[j] = temp[i -  1 - j];
    }

    buf[i] = '\0';

    return i;
}

size_t str_of_i(char *buf, int32_t i) {
    size_t written = 0;

    uint32_t uv;
    char *b = buf;

    if (i < 0) {
        if ((uint32_t)i == 0x80000000) {
            uv = (uint32_t)i;
        } else {
            uv = (uint32_t)(i * -1);
        }

        b[0] = '-';

        b++;
        written++;
    } else {
        // Positive number should be OK to cast.
        uv = (uint32_t)i;
    }

    return written + str_of_u(b, uv);
}

size_t str_of_hex(char *buf, uint32_t u, uint8_t hex_digs) {
    uint32_t uv = u;
    for (uint8_t i = 0; i < hex_digs; i++) {
        uint8_t hex_dig_val = (uint8_t)uv & 0xF;
        buf[hex_digs - 1 - i] = hex_dig_val < 10 
            ? '0' + hex_dig_val 
            : 'A' + (hex_dig_val - 10);

        uv >>= 4;
    }

    buf[hex_digs] = '\0';

    return hex_digs;
}


