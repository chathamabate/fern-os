
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/test/process.h"
#include "s_util/constraints.h"

#include "k_startup/page.h"

#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

static void *fake_entry(void *arg) {
    (void)arg;
    return NULL;
}

#include "s_util/test.h"

static size_t init_alloc_blocks;
static bool pretest(void) {
    init_alloc_blocks = al_num_user_blocks(get_default_allocator());
    TEST_SUCCEED();
}
static bool posttest(void) {
    size_t final_alloc_blocks = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_HEX(init_alloc_blocks, final_alloc_blocks);
    TEST_SUCCEED();
}

/*
 * These tests will play around with the thread and process structures, and confirm creation/
 * deletion still works. We are kinda simulating how the kernel will interact with these structures.
 */

static bool test_new_and_delete(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    delete_process(proc);

    TEST_SUCCEED();
}

static bool test_new_thread(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *thr0 = proc_new_thread(proc, fake_entry, NULL);
    TEST_TRUE(thr0 != NULL);
    TEST_EQUAL_HEX(THREAD_STATE_DETATCHED, thr0->state);
    TEST_EQUAL_HEX(proc->main_thread, thr0);

    thread_t *thr1 = proc_new_thread(proc, fake_entry, NULL);
    TEST_TRUE(thr1 != NULL);
    TEST_EQUAL_HEX(THREAD_STATE_DETATCHED, thr1->state);
    TEST_TRUE(thr1 != proc->main_thread);

    delete_process(proc);

    TEST_SUCCEED();
}

static bool test_many_threads(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    for (size_t i = 0; i < FOS_MAX_THREADS_PER_PROC; i++) {
        thread_t *thr = proc_new_thread(proc, fake_entry, NULL);
        TEST_TRUE(thr != NULL);
    }

    thread_t *null_thr = proc_new_thread(proc, fake_entry, NULL);
    TEST_EQUAL_HEX(NULL, null_thr);

    // Now, make sure all added threads make sense.

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    idtb_reset_iterator(proc->thread_table);
    for (thread_id_t tid = idtb_get_iter(proc->thread_table); tid != NULL_TID;
            tid = idtb_next(proc->thread_table)) {
        thread_t *thr = (thread_t *)idtb_get(proc->thread_table, tid);

        TEST_TRUE(thr != NULL);
        TEST_EQUAL_HEX(tid, thr->tid);
        TEST_EQUAL_HEX(THREAD_STATE_DETATCHED, thr->state);
        TEST_EQUAL_HEX(proc, thr->proc);
    }

    delete_process(proc);

    TEST_SUCCEED();
}


bool test_process(void) {
    BEGIN_SUITE("Process");
    RUN_TEST(test_new_and_delete);
    RUN_TEST(test_new_thread);
    RUN_TEST(test_many_threads);
    return END_SUITE();
}
