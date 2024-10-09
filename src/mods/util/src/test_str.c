
#include "util/str.h"
#include "msys/test.h"

static bool test_str_eq(void) {
    TEST_TRUE(str_eq("Hello", "Hello")); 
    TEST_TRUE(str_eq("H", "H")); 
    TEST_TRUE(str_eq("4294967295","4294967295"));
    TEST_FALSE(str_eq("A", "H")); 
    TEST_FALSE(str_eq("Hell", "Hello"));
    TEST_FALSE(str_eq("HEllo", "Hello")); 
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

typedef struct _test_str_of_hex_case_t {
    uint32_t in_val;
    uint8_t digits;
    const char *exp_str;
} test_str_of_hex_case_t;

static const test_str_of_hex_case_t TEST_STR_OF_HEX_CASES[] = {
    {
        .in_val = 0xF,
        .digits = 1,
        .exp_str = "F"
    },
    {
        .in_val = 0xF,
        .digits = 3,
        .exp_str = "00F"
    },
    {
        .in_val = 0xAA1100,
        .digits = 8,
        .exp_str = "00AA1100"
    },
};
static const size_t TEST_STR_OF_HEX_CASES_LEN = 
    sizeof(TEST_STR_OF_HEX_CASES) / sizeof(test_str_of_hex_case_t);

static bool test_str_of_hex(void) {
    char buf[20];
    for (size_t i = 0; i < TEST_STR_OF_HEX_CASES_LEN; i++) {
        const test_str_of_hex_case_t *c = &(TEST_STR_OF_HEX_CASES[i]);
        size_t chars = str_of_hex(buf, c->in_val, c->digits);
        TEST_TRUE(str_eq(buf, c->exp_str));
        TEST_TRUE(chars == c->digits);
    }
    TEST_SUCCEED();
}

bool test_str(void) {
    TEST_TRUE(test_str_eq());
    TEST_TRUE(test_str_of_u());
    TEST_TRUE(test_str_of_i());
    TEST_TRUE(test_str_of_hex());

    TEST_SUCCEED();
}
