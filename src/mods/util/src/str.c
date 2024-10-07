
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

size_t str_cpy(char *dest, const char *src) {
    char *d_iter = dest;
    const char *s_iter = src;
    while ((*(d_iter++) = *(s_iter++)));
    return (d_iter - dest) - 1;
}

size_t str_of_u_ra(char *buf, size_t n, char pad, uint32_t u) {
    if (n == 0) {
        return 0;
    }

    int i = n - 1; 

    if (u == 0) {
        buf[i--] = '0';
    } else {
        uint32_t uv = u;
        while (uv > 0 && i >= 0) {
            buf[i--] = '0' + (uv % 10);
            uv /= 10;
        }
    }

    for (int j = i; j >= 0; j--) {
        buf[j] = pad;
    }

    buf[n] = '\0';

    return i + 1;
}

size_t str_of_u(char *buf, uint32_t u) {
    char temp_buf[11];
    size_t pads_written = str_of_u_ra(temp_buf, 10, 'X', u);
    return str_cpy(buf, temp_buf + pads_written);
}

size_t str_of_i_ra(char *buf, size_t n, char pad, int32_t i) {
    bool was_neg;
    uint32_t uv;

    if (i < 0) {
        if ((uint32_t)i == 0x80000000) {
            uv = (uint32_t)i;
        } else {
            uv = (uint32_t)(i * -1);
        }

        was_neg = true;
    } else {
        // Positive number should be OK to cast.
        uv = (uint32_t)i;
        was_neg = false;
    }

    size_t pads_written = str_of_u_ra(buf, n, pad, uv);

    if (pads_written > 0 && was_neg) {
        pads_written--;
        buf[pads_written] = '-';
    }

    return pads_written;
}

size_t str_of_i(char *buf, int32_t i) {
    char temp_buf[12];
    size_t pads_written = str_of_i_ra(temp_buf, 11, 'X', i);
    return str_cpy(buf, temp_buf + pads_written);
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


