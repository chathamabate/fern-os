
#include "s_data/wait_queue.h"
#include "s_data/test/wait_queue.h"

#include "k_bios_term/term.h"
#include "s_util/err.h"
#include <stdint.h>


static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

static bool test_bwq_create_and_delete(void) {
    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_simple(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    // Popping now should return empty.
    err = bwq_pop(bwq, NULL);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    err = bwq_enqueue(bwq, (void *)1);
    // Waiting: [1] Ready: []

    void *res;

    // Still nothing ready.
    err = bwq_pop(bwq, &res);
    TEST_EQUAL_HEX(FOS_EMPTY, err);
    TEST_EQUAL_HEX(NULL, res);

    err = bwq_notify(bwq, BWQ_NOTIFY_NEXT);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    // Waiting: [] Ready: [1]

    err = bwq_pop(bwq, &res);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX((void *)1, res);

    // Waiting: [] Ready: []

    err = bwq_pop(bwq, NULL);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_notify_types(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 10; i++) {
        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1...10] Ready: []

    for (int i = 1; i <= 5; i++) {
        err = bwq_notify(bwq, i % 2 == 0 ? BWQ_NOTIFY_LAST : BWQ_NOTIFY_NEXT);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [4 ... 8] Ready: [1, 10, 2, 9, 3]

    err = bwq_notify(bwq, BWQ_NOTIFY_ALL);
    
    // Waiting: [] Ready: [1, 10, 2, 9, 3, 4...8]

    int expected[10] = {
        1, 10, 2, 9, 3, 4, 5, 6, 7, 8
    };

    intptr_t res;
    for (int i = 0; i < 10; i++) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_INT(expected[i], res);
    }

    err = bwq_pop(bwq, NULL);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_big(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 100; i++) {
        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bwq_enqueue(bwq, (void *)(i * 2));
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1...100] Ready: [2,4,6...200]

    intptr_t res;
    for (int i = 1; i <= 100; i++) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_INT(i * 2, res);
    }

    // Waiting: [1...100] Ready: []

    for (int i = 0; i < 100; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_NEXT);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [] Ready: [1...100]

    for (int i = 1; i <= 100; i++) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_INT(i, res);
    }

    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_remove(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 20; i++) {
        err = bwq_enqueue(bwq, (void *)i);    
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1...20] Ready: []

    for (int i = 0; i < 10; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1...10] Ready: [20...11]

    for (int i = 1; i <= 20; i += 2) {
        wq_remove((wait_queue_t *)bwq, (void *)i);
    }

    // Waiting [2,4...10] Ready: [20,18...12]
    
    intptr_t res;
    for (int i = 20; i > 10; i -= 2) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_INT(i, res);
    }

    // Waiting [2,4...10] Ready: []
    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_EMPTY, err);
    TEST_EQUAL_INT(0, res);

    err = bwq_notify(bwq, BWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (int i = 2; i <= 10; i += 2) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
        TEST_EQUAL_INT(i, res);
    }

    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    // Waiting: [] Ready: []

    delete_wait_queue((wait_queue_t *)bwq);
    TEST_SUCCEED();
}

static bool test_bwq_remove_multiple(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 5; i++) {
        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1, 1, 2, 2 ... 5, 5] Ready: []

    wq_remove((wait_queue_t *)bwq, (void *)2);
    wq_remove((wait_queue_t *)bwq, (void *)4);

    // Waiting: [1, 1, 3, 3, 5, 5] Ready: []
    
    for (int i = 0; i < 3; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Waiting: [1, 1, 3] [3, 5, 5]

    wq_remove((wait_queue_t *)bwq, (void *)5);

    // Waiting: [1, 1, 3] [3]
    
    intptr_t res;
    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_INT(3, res);

    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_EMPTY, err);

    // Waiting: [1, 1, 3] []

    err = bwq_notify(bwq, BWQ_NOTIFY_NEXT);

    // Waiting: [1, 3] [1]

    // I guess just delete, make sure our destructor can handle a non-empty state.

    delete_wait_queue((wait_queue_t *)bwq);
    TEST_SUCCEED();
}


bool test_basic_wait_queue(void) {
    BEGIN_SUITE("Basic Wait Queue");

    RUN_TEST(test_bwq_create_and_delete);
    RUN_TEST(test_bwq_simple);
    RUN_TEST(test_bwq_notify_types);
    RUN_TEST(test_bwq_big);
    RUN_TEST(test_bwq_remove);
    RUN_TEST(test_bwq_remove_multiple);

    return END_SUITE();
}

