
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
 *
 * Remember, process.c has no access to scheduling, so these tests are somewhat limited.
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

    // Try deleting every other thread just to see what happens.

    for (thread_id_t tid = 0; tid < FOS_MAX_THREADS_PER_PROC; tid += 2) {
        thread_t *thr = idtb_get(proc->thread_table, tid);
        if (thr) {
            proc_delete_thread(proc, thr, true);
        }
    }

    delete_process(proc);

    TEST_SUCCEED();
}

static bool test_fork_process(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *main_thread = proc_new_thread(proc, fake_entry, NULL);
    TEST_TRUE(main_thread != NULL);

    thread_t *secondary_thread = proc_new_thread(proc, fake_entry, NULL);
    TEST_TRUE(secondary_thread != NULL);

    process_t *child_proc = new_process_fork(proc, secondary_thread, 1);
    TEST_TRUE(child_proc != NULL);

    TEST_EQUAL_HEX(1, child_proc->pid);
    TEST_EQUAL_HEX(proc, child_proc->parent);

    // We should get the secondary thread but not the first thread!
    TEST_EQUAL_HEX(NULL, idtb_get(child_proc->thread_table, main_thread->tid));
    TEST_TRUE(NULL != idtb_get(child_proc->thread_table, secondary_thread->tid));

    TEST_EQUAL_HEX(secondary_thread->tid, child_proc->main_thread->tid);

    delete_process(child_proc);
    delete_process(proc);

    TEST_SUCCEED();
}

/**
 * Test out making invlaid futexes.
 */
static bool test_simple_futex(void) {
    fernos_error_t err;

    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    const void *true_e;
    err = pd_alloc_pages(pd, true, (void *)M_4K, (const void *)(M_4K + M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    futex_t *u_futex0 = (futex_t *)M_4K;
    futex_t *u_futex1 = (futex_t *)M_4K - 1;

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    err = proc_register_futex(proc, u_futex0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = proc_register_futex(proc, u_futex1);
    TEST_TRUE(err != FOS_SUCCESS);

    err = proc_register_futex(proc, u_futex0);
    TEST_TRUE(err != FOS_SUCCESS);

    proc_deregister_futex(proc, u_futex0);

    err = proc_register_futex(proc, u_futex0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    delete_process(proc);

    TEST_SUCCEED();
}

/**
 * This test is going to populate the process structures in a way that the kernel is intended to.
 */
static bool test_complex_process(void) {
    fernos_error_t err;

    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    const void *true_e;
    err = pd_alloc_pages(pd, true, (void *)M_4K, (const void *)(M_4K + M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    futex_t *u_futex0 = (futex_t *)M_4K;
    futex_t *u_futex1 = (futex_t *)M_4K + 1;

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *threads[10];
    const size_t NUM_THREADS = sizeof(threads) / sizeof(threads[0]);

    for (size_t i = 0; i < NUM_THREADS; i++) {
        thread_t *thr = proc_new_thread(proc, fake_entry, NULL);
        TEST_TRUE(thr != NULL);
        threads[i] = thr;
    }

    // Ok, now maybe make a new futex??
    
    err = proc_register_futex(proc, u_futex0);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = proc_register_futex(proc, u_futex1);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    basic_wait_queue_t *wq = proc_get_futex_wq(proc, u_futex1);
    TEST_TRUE(wq != NULL);

    for (size_t i = 0; i < NUM_THREADS; i += 2) {
        thread_t *thr = threads[i];

        err = bwq_enqueue(wq, thr);

        thr->state = THREAD_STATE_WAITING;
        thr->wq = (wait_queue_t *)wq;
        thr->u_wait_ctx = NULL;
    }

    delete_process(proc);
    TEST_SUCCEED();
}


bool test_process(void) {
    BEGIN_SUITE("Process");
    RUN_TEST(test_new_and_delete);
    RUN_TEST(test_new_thread);
    RUN_TEST(test_many_threads);
    RUN_TEST(test_fork_process);
    RUN_TEST(test_simple_futex);
    RUN_TEST(test_complex_process);
    return END_SUITE();
}
