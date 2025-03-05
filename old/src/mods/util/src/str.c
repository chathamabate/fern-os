
#include "util/str.h"
#include "msys/test.h"
#include <stdarg.h>


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
    size_t i = 0;
    while (true) {
        char c = src[i];
        dest[i] = c;

        if (c == '\0') {
            return i; // We don't count \0 in the length returned.
        }

        i++;
    }
}

size_t str_len(const char *src) {
    size_t i = 0;
    while (true) {
        if (src[i] == '\0') {
            return i;
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

void str_la(char *buf, size_t n, char pad, const char *s) {
    buf[n] = '\0';

    size_t i = 0;

    while (i < n && s[i]) {
        buf[i] = s[i];
        i++;
    }

    while (i < n) {
        buf[i] = pad;
        i++;
    }
}

void str_ra(char *buf, size_t n, char pad, const char *s) {
    size_t len = str_len(s);

    size_t i = 0;

    while (i < n && i < len) {
        buf[n - 1 - i] = s[len - 1 - i];
        i++;
    }

    while (i < n) {
        buf[n - 1 - i] = pad;
        i++;
    }

    buf[n] = '\0';
}

void str_center(char *buf, size_t n, char pad, const char *s) {
    size_t len = str_len(s);

    if (len <= n) {
        size_t pads = n - len;
        size_t prefix_pads = pads / 2;

        for (size_t i = 0; i < prefix_pads; i++) {
            buf[i] = pad;
        }
        
        for (size_t i = 0; i < len; i++) {
            buf[prefix_pads + i] = s[i];
        }

        for (size_t i = prefix_pads + len; i < n; i++) {
            buf[i] = pad;
        }
    } else {
        // n < len (There will be cutoff)
        size_t cutoff_prefix = (len - n) / 2;
        for (size_t i = 0; i < n; i++) {
            buf[i] = s[cutoff_prefix + i];
        }
    }

    buf[n] = '\0';
}

size_t str_fmt(char *buf, const char *fmt,...) {
    va_list va; 
    va_start(va, fmt);
    size_t chars = str_vfmt(buf, fmt, va);
    va_end(va);

    return chars;
}

size_t str_vfmt(char *buf, const char *fmt, va_list va) {
    char *buf_ptr = buf;
    const char *fmt_ptr = fmt;

    uint32_t uval;
    int32_t ival;
    const char *sval;

    char c;
    while ((c = *(fmt_ptr++)) != '\0') {
        if (c != '%') {
            *(buf_ptr++) = c;
            continue;
        } 

        char f = *(fmt_ptr++);
        if (f == '\0') {
            break;
        }

        switch (f) {
        case '%':
            *(buf_ptr++) = '%';
            break;
        case 'X':
            uval = va_arg(va, uint32_t);
            buf_ptr += str_of_hex(buf_ptr, uval, 8);
            break;
        case 'u':
            uval = va_arg(va, uint32_t);
            buf_ptr += str_of_u(buf_ptr, uval);
            break;
        case 'd':
            ival = va_arg(va, int32_t);
            buf_ptr += str_of_i(buf_ptr, ival);
            break;
        case 's':
            sval = va_arg(va, const char *);
            buf_ptr += str_cpy(buf_ptr, sval);
            break;
        default:
            break;
        }
    }

    *buf_ptr = '\0';

    return (buf_ptr - buf);
}


