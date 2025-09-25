
#include "s_data/wait_queue.h"
#include "s_data/test/wait_queue.h"

#include "k_startup/vga_term.h"
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
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    err = bwq_enqueue(bwq, (void *)1);
    // Waiting: [1] Ready: []

    void *res;

    // Still nothing ready.
    err = bwq_pop(bwq, &res);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    TEST_EQUAL_HEX(NULL, res);

    err = bwq_notify(bwq, BWQ_NOTIFY_NEXT);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting: [] Ready: [1]

    err = bwq_pop(bwq, &res);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_HEX((void *)1, res);

    // Waiting: [] Ready: []

    err = bwq_pop(bwq, NULL);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_notify_types(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 10; i++) {
        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1...10] Ready: []

    for (int i = 1; i <= 5; i++) {
        err = bwq_notify(bwq, i % 2 == 0 ? BWQ_NOTIFY_LAST : BWQ_NOTIFY_NEXT);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
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
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_INT(expected[i], res);
    }

    err = bwq_pop(bwq, NULL);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    delete_wait_queue((wait_queue_t *)bwq);

    TEST_SUCCEED();
}

static bool test_bwq_big(void) {
    fernos_error_t err;

    basic_wait_queue_t *bwq = new_da_basic_wait_queue();
    TEST_TRUE(bwq != NULL);

    for (int i = 1; i <= 100; i++) {
        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        err = bwq_enqueue(bwq, (void *)(i * 2));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1...100] Ready: [2,4,6...200]

    intptr_t res;
    for (int i = 1; i <= 100; i++) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_INT(i * 2, res);
    }

    // Waiting: [1...100] Ready: []

    for (int i = 0; i < 100; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_NEXT);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [] Ready: [1...100]

    for (int i = 1; i <= 100; i++) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
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
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1...20] Ready: []

    for (int i = 0; i < 10; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1...10] Ready: [20...11]

    for (int i = 1; i <= 20; i += 2) {
        wq_remove((wait_queue_t *)bwq, (void *)i);
    }

    // Waiting [2,4...10] Ready: [20,18...12]
    
    intptr_t res;
    for (int i = 20; i > 10; i -= 2) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_INT(i, res);
    }

    // Waiting [2,4...10] Ready: []
    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    TEST_EQUAL_INT(0, res);

    err = bwq_notify(bwq, BWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (int i = 2; i <= 10; i += 2) {
        err = bwq_pop(bwq, (void **)&res);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        TEST_EQUAL_INT(i, res);
    }

    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

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
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        err = bwq_enqueue(bwq, (void *)i);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1, 1, 2, 2 ... 5, 5] Ready: []

    wq_remove((wait_queue_t *)bwq, (void *)2);
    wq_remove((wait_queue_t *)bwq, (void *)4);

    // Waiting: [1, 1, 3, 3, 5, 5] Ready: []
    
    for (int i = 0; i < 3; i++) {
        err = bwq_notify(bwq, BWQ_NOTIFY_LAST);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting: [1, 1, 3] [3, 5, 5]

    wq_remove((wait_queue_t *)bwq, (void *)5);

    // Waiting: [1, 1, 3] [3]
    
    intptr_t res;
    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_INT(3, res);

    err = bwq_pop(bwq, (void **)&res);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

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

static bool test_vwq_create_and_delete(void) {
    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);
    delete_wait_queue((wait_queue_t *)vwq);

    TEST_SUCCEED();
}

static bool test_vwq_simple(void) {
    fernos_error_t err;

    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);

    err = vwq_enqueue(vwq, (void *)1, 0);
    TEST_TRUE(err != FOS_E_SUCCESS);

    err = vwq_enqueue(vwq, (void *)1, 1 << 1);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = vwq_enqueue(vwq, (void *)2, 1 << 2);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = vwq_enqueue(vwq, (void *)3, (1 << 1) | (1 << 2));
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting [1, 2, 3] Ready []

    err = vwq_pop(vwq, NULL, NULL); 
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    err = vwq_notify(vwq, 2, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting [1] Ready [2:2, 3:2]

    err = vwq_notify(vwq, 1, VWQ_NOTIFY_FIRST);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting [] Ready [2:2, 3:2, 1:1]

    intptr_t res;
    uint8_t ready_id;

    err = vwq_pop(vwq, (void **)&res, &ready_id);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_INT(2, res);
    TEST_EQUAL_UINT(2, ready_id);

    // Waiting [] Ready [3:2, 1:1]

    err = vwq_pop(vwq, (void **)&res, &ready_id);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_INT(3, res);
    TEST_EQUAL_UINT(2, ready_id);

    // Waiting [] Ready [1:1]
    
    err = vwq_pop(vwq, (void **)&res, &ready_id);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_INT(1, res);
    TEST_EQUAL_UINT(1, ready_id);

    delete_wait_queue((wait_queue_t *)vwq);

    TEST_SUCCEED();
}

static bool test_vwq_notify_types(void) {
    fernos_error_t err;

    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);

    for (int i = 1; i <= 10; i++) {
        err = vwq_enqueue(vwq, (void *)i, 1 << i);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        err = vwq_enqueue(vwq, (void *)(i + 10), (1 << i) | (1 << 0));
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting [1:(1), 11:(0,1), 2:(2), 12:(0,2)... 10:(10), 20:(0,10)] Ready: []

    for (int i = 2; i <= 10; i += 2) {
        err = vwq_notify(vwq, i, VWQ_NOTIFY_ALL);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting [...] Ready: [2:2, 12:2, 4:4, 14:4, ...10:10, 20:10]

    err = vwq_notify(vwq, 0, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    intptr_t res;
    uint8_t ready_id;

    for (int i = 2; i <= 10; i += 2) {
        err = vwq_pop(vwq, (void **)&res, &ready_id);    
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        TEST_EQUAL_INT(i, res);
        TEST_EQUAL_INT(i, ready_id);

        err = vwq_pop(vwq, (void **)&res, &ready_id);    
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        TEST_EQUAL_INT(i + 10, res);
        TEST_EQUAL_INT(i, ready_id);
        
    }

    // Waiitng: [1:(1), 3:(3)... 9:(9)] Ready: [11:0, 13:0, 15:0 ... 19:0]
    
    for (int i = 11; i < 20; i += 2) {
        err = vwq_pop(vwq, (void **)&res, &ready_id);    
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        TEST_EQUAL_INT(i, res);
        TEST_EQUAL_INT(0, ready_id);
    }

    // Waiitng: [1:(1), 3:(3)... 9:(9)] Ready: []

    err = vwq_pop(vwq, (void **)&res, &ready_id);    
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    TEST_EQUAL_HEX(NULL, (void *)res);
    TEST_EQUAL_HEX(VWQ_NULL_READY_ID, ready_id);

    // Hopefully this successfully deletes the contents of the wait queue.

    delete_wait_queue((wait_queue_t *)vwq);

    TEST_SUCCEED();
}

static bool test_vwq_big(void) {
    fernos_error_t err;

    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);

    for (int i = 1; i <= 100; i++) {
        err = vwq_enqueue(vwq, (void *)i, 1 << 0);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    for (int i = 1; i <= 100; i++) {
        err = vwq_enqueue(vwq, (void *)i, 1 << 1);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    err = vwq_notify(vwq, 1, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    for (int i = 1; i <= 50; i++) {
        err = vwq_notify(vwq, 0, VWQ_NOTIFY_FIRST);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    }

    // Waiting [51...100:0] Ready[1...100:1 1...50:0]
    
    intptr_t res;
    uint8_t ready_id;
    for (int i = 1; i <= 100; i++) {
        err = vwq_pop(vwq, (void **)&res, &ready_id);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

        TEST_EQUAL_INT(i, res);
        TEST_EQUAL_UINT(1, ready_id);
    }

    err = vwq_pop(vwq, (void **)&res, &ready_id);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    TEST_EQUAL_INT(1, res);
    TEST_EQUAL_UINT(0, ready_id);

    // Now try deleting with a non-empty waiting and ready queue.

    delete_wait_queue((wait_queue_t *)vwq);

    TEST_SUCCEED();
}

static bool test_vwq_remove(void) {
    fernos_error_t err;

    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);


    for (int i = 1; i <= 5; i++) {
        err = vwq_enqueue(vwq, (void *)i, 1 << 1);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err); 
    }

    // Waiting: [1..5:1] Ready: []
    
    for (int i = 1; i <= 5; i++) {
        err = vwq_enqueue(vwq, (void *)i, 1 << 2);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err); 
    }
    
    // Waiting: [1..5:1 1...5:2] Ready: []

    wq_remove((wait_queue_t *)vwq, (void *)1);

    // Waiting: [2..5:1 2..5:2] Ready: []

    err = vwq_notify(vwq, 2, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting: [2...5:1] Ready: [2...5:2]

    wq_remove((wait_queue_t *)vwq, (void *)5);

    // Waiting: [2...4:1] Ready: [2...4:2]
    
    err = vwq_notify(vwq, 1, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    // Waiting: [] Ready: [2...4:2, 2...4:1]

    intptr_t res;
    uint8_t ready_id;
    for (int i = 2; i >= 1; i--) {
        for (int j = 2; j <= 4; j++) {
            err = vwq_pop(vwq, (void **)&res, &ready_id);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            TEST_EQUAL_INT(j, res);
            TEST_EQUAL_UINT(i, ready_id);
        }
    }

    err = vwq_pop(vwq, (void **)&res, &ready_id);
    TEST_EQUAL_HEX(FOS_E_EMPTY, err);

    delete_wait_queue((wait_queue_t *)vwq);

    TEST_SUCCEED();
}

static bool test_vwq_remove_seq(void) {
    fernos_error_t err;

    vector_wait_queue_t *vwq = new_da_vector_wait_queue();
    TEST_TRUE(vwq != NULL);

    for (int i = 1; i <= 5; i++) {
        for (int j = 0; j < 3; j++) {
            err = vwq_enqueue(vwq, (void *)i, 1 << 0);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        }
    }

    // Waiting: [1, 1, 1, 2, 2, 2... 5, 5, 5:0] Ready: []

    err = vwq_notify(vwq, 0, VWQ_NOTIFY_FIRST);
    TEST_EQUAL_HEX(err, FOS_E_SUCCESS);

    wq_remove((wait_queue_t *)vwq, (void *)1);
    wq_remove((wait_queue_t *)vwq, (void *)2);

    err = vwq_notify(vwq, 0, VWQ_NOTIFY_ALL);
    TEST_EQUAL_HEX(err, FOS_E_SUCCESS);

    // Waiting [] Ready [333,444,555]

    wq_remove((wait_queue_t *)vwq, (void *)5);

    intptr_t res;
    uint8_t ready_id;
    for (int i = 3; i <= 4; i++) {
        for (int j = 0; j < 3; j++) {
            err = vwq_pop(vwq, (void **)&res, &ready_id);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
            TEST_EQUAL_INT(i, res);
            TEST_EQUAL_UINT(0, ready_id);
        }
    }

    delete_wait_queue((wait_queue_t *)vwq);
    TEST_SUCCEED();
}

// Hmmm, what else, test remove maybe?? That could be cool ig...

bool test_vector_wait_queue(void) {
    BEGIN_SUITE("Vector Wait Queue");

    RUN_TEST(test_vwq_create_and_delete);
    RUN_TEST(test_vwq_simple);
    RUN_TEST(test_vwq_notify_types);
    RUN_TEST(test_vwq_big);
    RUN_TEST(test_vwq_remove);
    RUN_TEST(test_vwq_remove_seq);

    return END_SUITE();
}

static bool test_twq_create_and_delete(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);
    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

/**
 * Try to enqueue all elements from s to e EXCLUSIVE using step size 'step'
 *
 * NOTE: e MUST be reached exactly be the step!
 */
static bool try_twq_enqueue_range(timed_wait_queue_t *twq, uint32_t s, uint32_t e, uint32_t step) {
    for (uint32_t i = s; i != e; i += step) {
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                twq_enqueue(twq, (void *)i, i));
    }

    TEST_SUCCEED();
}

static bool try_twq_pop_range(timed_wait_queue_t *twq, uint32_t s, uint32_t e, uint32_t step) {
    void *p;

    for (uint32_t i = s; i != e; i += step) {
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                twq_pop(twq, &p));
        TEST_EQUAL_HEX((void *)i, p);
    }

    TEST_SUCCEED();
}

static bool test_twq_simple0(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);

    TEST_TRUE(try_twq_enqueue_range(twq, 1, 11, 1));

    for (uint32_t i = 1; i <= 10; i++) {
        TEST_EQUAL_HEX(FOS_E_SUCCESS,
                twq_notify(twq, i));

        void *p;

        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                twq_pop(twq, &p));

        TEST_EQUAL_HEX((void *)i, p);
    }

    TEST_EQUAL_HEX(FOS_E_EMPTY,
            twq_pop(twq, NULL));

    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

static bool test_twq_simple1(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);

    TEST_TRUE(try_twq_enqueue_range(twq, 10, 0, -1));

    TEST_EQUAL_HEX(FOS_E_SUCCESS, 
            twq_notify(twq, 12));

    TEST_TRUE(try_twq_pop_range(twq, 1, 11, 1));

    TEST_EQUAL_HEX(FOS_E_EMPTY,
            twq_pop(twq, NULL));

    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

static bool test_twq_wrap(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);

    TEST_EQUAL_HEX(FOS_E_SUCCESS,
            twq_notify(twq, UINT32_MAX - 9));

    TEST_TRUE(try_twq_enqueue_range(twq, UINT32_MAX - 9, 11, 1));

    TEST_EQUAL_HEX(FOS_E_SUCCESS, 
            twq_notify(twq, 3));

    TEST_TRUE(try_twq_pop_range(twq, UINT32_MAX - 9, 4, 1));

    // There should be some left over, and that's OK!

    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

static bool test_twq_complex0(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);

    void *p;

    TEST_TRUE(try_twq_enqueue_range(twq, 0, 22, 2));

    // 0 should immediately be moved to the ready queue.

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_pop(twq, &p));
    TEST_EQUAL_HEX((void *)0, p);
    TEST_EQUAL_HEX(FOS_E_EMPTY, twq_pop(twq, NULL));

    // W: 2,4,6 ... 20 R:

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_notify(twq, 8));

    TEST_TRUE(try_twq_pop_range(twq, 2, 6, 2));

    // W: 10,12 ... 20
    // R: 6,8

    TEST_TRUE(try_twq_enqueue_range(twq, 19, 7, -2));

    // W: 9,10,11 ... 20
    // R: 6,8

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_notify(twq, 10));
    uint32_t exp[4] = {6, 8, 9, 10 };
    for (uint32_t i = 0; i < 4; i++) {
        TEST_EQUAL_HEX(FOS_E_SUCCESS,
                twq_pop(twq, &p));
        TEST_EQUAL_HEX((void *)(exp[i]), p);
    }

    // W: 11,12,13 ... 20
    // R: 

    TEST_EQUAL_HEX(FOS_E_SUCCESS,
            twq_enqueue(twq, (void *)0, 0));
    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_notify(twq, 1));

    // R: 11,12,13 ... 20, 0

    TEST_TRUE(try_twq_pop_range(twq, 11, 21, 1));

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_pop(twq, &p));
    TEST_EQUAL_HEX((void *)0, p);

    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

static bool test_twq_remove(void) {
    timed_wait_queue_t *twq = new_da_timed_wait_queue();
    TEST_TRUE(twq != NULL);


    TEST_TRUE(try_twq_enqueue_range(twq, 1, 4, 1));

    // W: 1 2 3 R:

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_enqueue(twq, (void *)2, 2));

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_notify(twq, 3));

    // W:  R: 1 2 2 3

    TEST_TRUE(try_twq_enqueue_range(twq, 1, 3, 1));

    // W:1 2  R: 1 2 2 3

    wq_remove((wait_queue_t *)twq, (void *)2);

    // W: 1 R: 1 3
    
    TEST_TRUE(try_twq_pop_range(twq, 1, 5, 2));

    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_notify(twq, 1));

    void *p;
    TEST_EQUAL_HEX(FOS_E_SUCCESS, twq_pop(twq, &p));
    TEST_EQUAL_HEX((void *)1, p);

    delete_wait_queue((wait_queue_t *)twq);

    TEST_SUCCEED();
}

bool test_timed_wait_queue(void) {
    BEGIN_SUITE("Timed Wait Queue");

    RUN_TEST(test_twq_create_and_delete);
    RUN_TEST(test_twq_simple0);
    RUN_TEST(test_twq_simple1);
    RUN_TEST(test_twq_wrap);
    RUN_TEST(test_twq_complex0);
    RUN_TEST(test_twq_remove);

    return END_SUITE();
}

