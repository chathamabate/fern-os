
#include "s_gfx/test/gfx_manager.h"
#include "s_util/misc.h"

static void (*logf)(const char *fmt, ...) = NULL;
#define LOGF_METHOD(...) logf(__VA_ARGS__)

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#include "s_util/test.h"

gfx_manager_t *(*gm_generator)(uint16_t width, uint16_t height) = NULL;

static uint32_t allocated_blocks;

static bool pretest(void) {
    allocated_blocks = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(allocated_blocks, al_num_user_blocks(get_default_allocator()));
    TEST_SUCCEED();
}

static bool test_new_and_delete(void) {
    gfx_manager_t *gm;

    gm = gm_generator(0, 0);
    TEST_TRUE(gm != NULL);
    TEST_EQUAL_UINT(0, gm_get_width(gm));
    TEST_EQUAL_UINT(0, gm_get_height(gm));
    delete_gfx_manager(gm);

    gm = gm_generator(100, 200);
    TEST_TRUE(gm != NULL);
    TEST_EQUAL_UINT(100, gm_get_width(gm));
    TEST_EQUAL_UINT(200, gm_get_height(gm));
    delete_gfx_manager(gm);

    TEST_SUCCEED();
}

static bool test_write_and_resize(void) {
    const uint16_t cases[][2] = {
        {1, 1},
        {0, 0},
        {100, 200},
        {200, 100},
        {0, 300},
        {100, 0},
        {800, 600}
    };
    const size_t num_cases = sizeof(cases) / sizeof(cases[0]);

    gfx_manager_t *gm = gm_generator(0, 0);
    TEST_TRUE(gm != NULL);

    for (size_t i = 0; i < num_cases; i++) {
        const uint16_t new_width = cases[i][0];
        const uint16_t new_height = cases[i][1];

        TEST_SUCCESS(gm_resize(gm, new_width, new_height));

        TEST_EQUAL_UINT(gm_get_width(gm), new_width);
        TEST_EQUAL_UINT(gm_get_height(gm), new_height);

        gfx_buffer_t front = gm_get_front(gm);

        TEST_EQUAL_UINT(front.width, new_width);
        TEST_EQUAL_UINT(front.height, new_height);

        mem_set(front.buffer, 0xFE, new_width * new_height * sizeof(gfx_color_t));
        TEST_TRUE(mem_chk(front.buffer, 0xFE, new_width * new_height * sizeof(gfx_color_t)));
    }

    delete_gfx_manager(gm);

    TEST_SUCCEED();
}

static bool test_back_and_swap(void) {
    gfx_manager_t *gm = gm_generator(100, 200);
    TEST_TRUE(gm != NULL);

    if (!gm_has_back(gm)) {
        // skip this test if the gm we are working with only has 1 buffer.
        delete_gfx_manager(gm);
        TEST_SUCCEED();
    }



    TEST_SUCCEED();
}

bool test_gfx_manager(const char *name, gfx_manager_t *(*gm_gen)(uint16_t, uint16_t), void (*l)(const char *fmt, ...)) {
    logf = l;
    gm_generator = gm_gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_new_and_delete);
    RUN_TEST(test_write_and_resize);
    RUN_TEST(test_back_and_swap);
    return END_SUITE();
}

bool test_dynamic_gfx_manager(void (*l)(const char *fmt, ...)) {
    return test_gfx_manager("Dynamic GFX Manager", new_da_dynamic_gfx_manager, l);
}

bool test_dynamic_gfx_manager_single(void (*l)(const char *fmt, ...)) {
    return test_gfx_manager("Dynamic GFX Manager Single", new_da_dynamic_gfx_manager_single, l);
}
