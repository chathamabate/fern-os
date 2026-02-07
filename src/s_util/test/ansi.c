

#include "k_startup/gfx.h"

// NOTE: This is a cyclic dependency, but we'll allow it.
#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"
#include "s_util/ansi.h"
#include "s_util/test/ansi.h"

static bool test_ansi_trim(void) {
    typedef struct {
        const char *s;
        const char *exp;
    } case_t;

    const case_t CASES[] = {
        { "", "" },
        { "a", "a" },
        {"\x1B[ABCDHelllo", ""},
        {"hello\x1B", "hello"},
        {"hello\x1B[]bcc", "hello"},
        {
            "\x1B[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 
            "\x1B[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        },
        {
            "aaaaaaaaaaaaaaaa\x1B[aaaaaaaaaaaa",
            "aaaaaaaaaaaaaaaa",
        },
        {
            "Hello\x1B[1234567890123456789",
            "Hello\x1B[1234567890123456789"
        },
        {
            "Hello\x1B[123456789012345678",
            "Hello"
        }
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    for (size_t i = 0; i < NUM_CASES; i++) {
        char buf[256];

        const case_t *c = CASES + i;
        TEST_TRUE(str_len(c->s) + 1 <= sizeof(buf));
        str_cpy(buf, c->s);

        size_t res = ansi_trim(buf);

        TEST_EQUAL_UINT(str_len(c->exp), res);
        TEST_TRUE(str_eq(c->exp, buf));
    }

    TEST_SUCCEED();
}

bool test_ansi(void) {
    BEGIN_SUITE("ANSI");
    RUN_TEST(test_ansi_trim);
    return END_SUITE();
}
