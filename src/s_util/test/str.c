
#include "k_bios_term/term.h"

// Allowing this for now.
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"
#include "s_util/str.h"
#include "s_util/test/str.h"

// Pretty simple tests ngl.
// Since making these, I have introduced numeric comparison macros,
// too lazy to copy them in here tho.

static bool test_mem_cmp(void) {
    uint8_t buf1[16] = { 1, 2, 4 };
    uint8_t buf2[16] = { 3, 2, 4 };

    TEST_TRUE(mem_cmp(buf1, buf2, 0));
    TEST_FALSE(mem_cmp(buf1, buf2, 3));

    buf2[0] = 1;
    TEST_TRUE(mem_cmp(buf1, buf2, 2));

    TEST_TRUE(mem_cmp(buf1, buf2, 0));

    TEST_SUCCEED();
}

static bool test_mem_cpy(void) {
    uint8_t dest[16];
    uint8_t src[16] = {1, 2, 3};

    mem_cpy(dest, src, 3);
    TEST_TRUE(mem_cmp(dest, src, 3));

    dest[0] = 4;
    mem_cpy(dest, src, 1);
    TEST_TRUE(mem_cmp(dest, src, 3));

    TEST_SUCCEED();
}

static bool test_str_eq(void) {
    TEST_TRUE(str_eq("", "")); 
    TEST_TRUE(str_eq("Hello", "Hello")); 
    TEST_TRUE(str_eq("H", "H")); 
    TEST_TRUE(str_eq("4294967295","4294967295"));
    TEST_FALSE(str_eq("A", "H")); 
    TEST_FALSE(str_eq("Hell", "Hello"));
    TEST_FALSE(str_eq("HEllo", "Hello")); 
    TEST_SUCCEED();
}

static bool test_str_cpy(void) {
    char buf[100];

    TEST_TRUE(str_cpy(buf, "heel") == 4);
    TEST_TRUE(str_eq(buf, "heel"));

    TEST_TRUE(str_cpy(buf, "h") == 1);
    TEST_TRUE(str_eq(buf, "h"));

    TEST_TRUE(str_cpy(buf, "") == 0);
    TEST_TRUE(str_eq(buf, ""));
    
    TEST_SUCCEED();
}

static bool test_str_len(void) {
    TEST_TRUE(str_len("HEllo") == 5);
    TEST_TRUE(str_len("H") == 1);
    TEST_TRUE(str_len("") == 0);

    TEST_SUCCEED();
}

typedef struct _test_str_of_u_case_t {
    uint32_t in_num;
    const char *exp_str;
    size_t exp_digits;
} test_str_of_u_case_t;

static const test_str_of_u_case_t TEST_STR_OF_U_CASES[] = {
    {
        .in_num = 0,
        .exp_str = "0",
        .exp_digits = 1
    },
    {
        .in_num = 1,
        .exp_str = "1",
        .exp_digits = 1 
    },
    {
        .in_num = 11,
        .exp_str = "11",
        .exp_digits = 2
    },
    {
        .in_num = 10,
        .exp_str = "10",
        .exp_digits = 2
    },
    {
        .in_num = 5402,
        .exp_str = "5402",
        .exp_digits = 4
    },
    {
        .in_num = 0xFFFFFFFFU,
        .exp_str = "4294967295",
        .exp_digits = 10
    },
};
static const size_t TEST_STR_OF_U_CASES_LEN = 
    sizeof(TEST_STR_OF_U_CASES) / sizeof(test_str_of_u_case_t);

static bool test_str_of_u(void) {
    char buf[20];
    for (size_t i = 0; i < TEST_STR_OF_U_CASES_LEN; i++) {
        const test_str_of_u_case_t *c = &(TEST_STR_OF_U_CASES[i]);
        size_t digits = str_of_u(buf, c->in_num);
        TEST_TRUE(str_eq(buf, c->exp_str));
        TEST_TRUE(digits == c->exp_digits);
    }
    TEST_SUCCEED();
}

typedef struct _test_str_of_i_case_t {
    int32_t in_num;
    const char *exp_str;
    size_t exp_chars;  // Includes the negative sign.
} test_str_of_i_case_t;

static const test_str_of_i_case_t TEST_STR_OF_I_CASES[] = {
    {
        .in_num = 0,
        .exp_str = "0",
        .exp_chars = 1
    },
    {
        .in_num = 1,
        .exp_str = "1",
        .exp_chars = 1 
    },
    {
        .in_num = 11,
        .exp_str = "11",
        .exp_chars = 2
    },
    {
        .in_num = 10,
        .exp_str = "10",
        .exp_chars = 2
    },
    {
        .in_num = 5402,
        .exp_str = "5402",
        .exp_chars = 4
    },
    {
        .in_num = 0xFFFFFFFF,
        .exp_str = "-1",
        .exp_chars = 2
    },
    {
        .in_num = -1342,
        .exp_str = "-1342",
        .exp_chars = 5 
    },
    {
        .in_num = 0x80000000,
        .exp_str = "-2147483648",
        .exp_chars = 11
    },
};
static const size_t TEST_STR_OF_I_CASES_LEN = 
    sizeof(TEST_STR_OF_I_CASES) / sizeof(test_str_of_i_case_t);

static bool test_str_of_i(void) {
    char buf[20];
    for (size_t i = 0; i < TEST_STR_OF_I_CASES_LEN; i++) {
        const test_str_of_i_case_t *c = &(TEST_STR_OF_I_CASES[i]);
        size_t chars = str_of_i(buf, c->in_num);
        TEST_TRUE(str_eq(buf, c->exp_str));
        TEST_TRUE(chars == c->exp_chars);
    }
    TEST_SUCCEED();
}

static bool test_str_of_hex_pad(void) {
    char buf[128];

    str_of_hex_pad(buf, 0, 0, ' ');
    TEST_TRUE(str_eq(buf, ""));

    str_of_hex_pad(buf, 0, 1, ' ');
    TEST_TRUE(str_eq(buf, "0"));

    str_of_hex_pad(buf, 0, 2, ' ');
    TEST_TRUE(str_eq(buf, " 0"));

    str_of_hex_pad(buf, 0x12, 3, '-');
    TEST_TRUE(str_eq(buf, "-12"));

    str_of_hex_pad(buf, 0x4455, 3, '-');
    TEST_TRUE(str_eq(buf, "455"));

    str_of_hex_pad(buf, 0x4455, 8, 'Z');
    TEST_TRUE(str_eq(buf, "ZZZZ4455"));

    TEST_SUCCEED();
}

static bool test_str_of_hex_no_pad(void) {
    char buf[128];

    str_of_hex_no_pad(buf, 0x0);
    TEST_TRUE(str_eq(buf, "0"));

    str_of_hex_no_pad(buf, 0x123);
    TEST_TRUE(str_eq(buf, "123"));

    str_of_hex_no_pad(buf, 0x12345678);
    TEST_TRUE(str_eq(buf, "12345678"));

    TEST_SUCCEED();
}

static bool test_str_la(void) {
    char buf[100];

    str_la(buf, 10, '-', "Hello");
    TEST_TRUE(str_eq(buf, "Hello-----"));

    str_la(buf, 5, '-', "Hello");
    TEST_TRUE(str_eq(buf, "Hello"));

    str_la(buf, 3, '-', "Hello");
    TEST_TRUE(str_eq(buf, "Hel"));

    str_la(buf, 3, '-', "");
    TEST_TRUE(str_eq(buf, "---"));

    TEST_SUCCEED();
}

static bool test_str_ra(void) {
    char buf[100];

    str_ra(buf, 10, '-', "Hello");
    TEST_TRUE(str_eq(buf, "-----Hello"));

    str_ra(buf, 5, '-', "Hello");
    TEST_TRUE(str_eq(buf, "Hello"));

    str_ra(buf, 3, '-', "Hello");
    TEST_TRUE(str_eq(buf, "llo"));

    str_ra(buf, 3, '-', "");
    TEST_TRUE(str_eq(buf, "---"));

    TEST_SUCCEED();
}

static bool test_str_center(void) {
    char buf[100];

    str_center(buf, 3, '-', "a");
    TEST_TRUE(str_eq(buf, "-a-"));

    str_center(buf, 3, '-', "ab");
    TEST_TRUE(str_eq(buf, "ab-"));

    str_center(buf, 3, '-', "ababa");
    TEST_TRUE(str_eq(buf, "bab"));

    str_center(buf, 5, '-', "ba");
    TEST_TRUE(str_eq(buf, "-ba--"));

    TEST_SUCCEED();
}

static bool test_str_fmt(void) {
    char buf[100];

    TEST_TRUE(str_fmt(buf, "Aye") == 3);
    TEST_TRUE(str_eq(buf, "Aye"));

    TEST_TRUE(str_fmt(buf, "A%ye") == 2);
    TEST_TRUE(str_eq(buf, "Ae"));

    TEST_TRUE(str_fmt(buf, "A%%ye") == 4);
    TEST_TRUE(str_eq(buf, "A%ye"));

    TEST_TRUE(str_fmt(buf, "Aye %s", "Oh") == 6);
    TEST_TRUE(str_eq(buf, "Aye Oh"));

    TEST_TRUE(str_fmt(buf, "%u %d", 12, -34) == 6);
    TEST_TRUE(str_eq(buf, "12 -34"));

    TEST_TRUE(str_fmt(buf, "Hello %X\n", 0x0AB) == 9);
    TEST_TRUE(str_eq(buf, "Hello AB\n"));

    TEST_TRUE(str_fmt(buf, "%2X", 0xF) == 2);
    TEST_TRUE(str_eq(buf, " F"));

    TEST_TRUE(str_fmt(buf, "N: %4X", 0xFFFFFF) == 7);
    TEST_TRUE(str_eq(buf, "N: FFFF"));

    TEST_TRUE(str_fmt(buf, "N: %04X", 0xF) == 7);
    TEST_TRUE(str_eq(buf, "N: 000F"));

    TEST_TRUE(str_fmt(buf, "N: %04X", 0x12345) == 7);
    TEST_TRUE(str_eq(buf, "N: 2345"));

    TEST_SUCCEED();
}

void test_str(void) {
    BEGIN_SUITE("Strings");
    RUN_TEST(test_mem_cmp);
    RUN_TEST(test_mem_cpy);
    RUN_TEST(test_str_eq);
    RUN_TEST(test_str_cpy);
    RUN_TEST(test_str_len);
    RUN_TEST(test_str_of_u);
    RUN_TEST(test_str_of_i);
    RUN_TEST(test_str_of_hex_pad);
    RUN_TEST(test_str_of_hex_no_pad);
    RUN_TEST(test_str_la);
    RUN_TEST(test_str_ra);
    RUN_TEST(test_str_center);
    RUN_TEST(test_str_fmt);
    END_SUITE();
}
