
#include "s_util/ansi.h"
#include "s_util/misc.h"

size_t ansi_trim(char *str) {
    const size_t slen = str_len(str);

    // Max number of character we'll search from the end of the string.
    const size_t lookback = MIN(slen, MAX_ANSI_SEQ_LEN);

    for (char *iter = str + slen - 1; iter >= str + slen - lookback; iter--) {
        if (*iter == '\x1B') {
            *iter = '\0';
            return (uintptr_t)iter - (uintptr_t)str;
        }
    }

    // Now CSI found in lookback region!
    return slen;
}
