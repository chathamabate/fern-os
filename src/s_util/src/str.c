
#include "s_util/str.h"
#include <stdarg.h>

// Consider rewriting these in assembly!

bool mem_cmp(const void *d0, const void *d1, size_t n) {
    const uint8_t *u0 = (const uint8_t *)d0;
    const uint8_t *u0_e = u0 + n;
    const uint8_t *u1 = (const uint8_t *)d1;

    while (u0 < u0_e) {
        if (*(u0++) != *(u1++)) {
            return false;
        }
    }

    return true;
}

void mem_cpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    uint8_t *e = d + n;
    const uint8_t *s = (const uint8_t *)src;

    while (d < e) {
        *(d++) = *(s++);
    }
}

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

// Gets number of leading zeros in a 32-bit hex value.
static size_t hex_leading_zeros(uint32_t u) {
    uint32_t mask = 0xF0000000;
    size_t lzs = 0;
    while (lzs < 7 && !(u & mask)) {
        mask >>= 4;
        lzs++;
    }

    return lzs;
}

size_t str_of_hex_pad(char *buf, uint32_t u, uint8_t digs, char pad) {
    size_t lzs = hex_leading_zeros(u);
    size_t sigs = 8 - lzs;

    if (sigs < digs) {
        lzs = digs - sigs;
    } else {
        sigs = digs;
        lzs = 0;
    }

    size_t i;

    for (i = 0; i < lzs; i++) {
        buf[i] = pad;
    }

    // Trim out all digits we don't care about.
    uint32_t uv = u << ((8 - sigs) * 4);

    for (; i < digs; i++) {
        uint32_t d = uv >> 28;
        buf[i] = d < 10 
            ? '0' + d
            : 'A' + (d - 10);

        uv <<= 4; 
    }

    buf[digs] = '\0';

    return digs;
}

size_t str_of_hex_no_pad(char *buf, uint32_t u) {
    size_t lzs = hex_leading_zeros(u);

    // The padding character should be ignored.
    return str_of_hex_pad(buf, u, 8 - lzs, ' ');
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

    // We'll parse padding for every formatter, but it will only 
    // do anything for hex as of now.
    bool zero_pad = false;
    uint8_t width = 0;

    char c;
    char f;
    while ((c = *(fmt_ptr++)) != '\0') {
        if (c != '%') {
            *(buf_ptr++) = c;
            continue;
        } 

        if ((f = *(fmt_ptr++)) == '\0') {
            break;
        }

        if (f == '0') {
            zero_pad = true;
            if ((f = *(fmt_ptr++)) == '\0') {
                break;
            }
        }

        if ('1' <= f && f <= '9') {
            width = f - '0';
            if ((f = *(fmt_ptr++)) == '\0') {
                break;
            }
        }

        switch (f) {
        case '%':
            *(buf_ptr++) = '%';
            break;
        case 'X':
            uval = va_arg(va, uint32_t);
            if (width > 0) {
                buf_ptr += str_of_hex_pad(buf_ptr, uval, width, zero_pad ? '0' : ' ');
            } else {
                buf_ptr += str_of_hex_no_pad(buf_ptr, uval);
            }
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


