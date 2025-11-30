
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/test/process.h"
#include "s_util/constraints.h"

#include "k_startup/page.h"
#include "k_startup/handle.h"

#include "k_startup/vga_cd.h"

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

    TEST_SUCCESS(delete_process(proc));

    TEST_SUCCEED();
}

static bool test_new_thread(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *thr0 = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
    TEST_TRUE(thr0 != NULL);
    TEST_EQUAL_HEX(THREAD_STATE_DETATCHED, thr0->state);
    TEST_EQUAL_HEX(proc->main_thread, thr0);

    thread_t *thr1 = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
    TEST_TRUE(thr1 != NULL);
    TEST_EQUAL_HEX(THREAD_STATE_DETATCHED, thr1->state);
    TEST_TRUE(thr1 != proc->main_thread);

    TEST_SUCCESS(delete_process(proc));

    TEST_SUCCEED();
}

static bool test_many_threads(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    for (size_t i = 0; i < FOS_MAX_THREADS_PER_PROC; i++) {
        thread_t *thr = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
        TEST_TRUE(thr != NULL);
    }

    thread_t *null_thr = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
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

    TEST_SUCCESS(delete_process(proc));

    TEST_SUCCEED();
}

static bool test_fork_process(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *main_thread = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
    TEST_TRUE(main_thread != NULL);

    thread_t *secondary_thread = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
    TEST_TRUE(secondary_thread != NULL);

    process_t *child_proc;
    fernos_error_t err = new_process_fork(proc, secondary_thread, 1, &child_proc);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_TRUE(child_proc != NULL);

    TEST_EQUAL_HEX(1, child_proc->pid);
    TEST_EQUAL_HEX(proc, child_proc->parent);

    // We should get the secondary thread but not the first thread!
    TEST_EQUAL_HEX(NULL, idtb_get(child_proc->thread_table, main_thread->tid));
    TEST_TRUE(NULL != idtb_get(child_proc->thread_table, secondary_thread->tid));

    TEST_EQUAL_HEX(secondary_thread->tid, child_proc->main_thread->tid);

    TEST_SUCCESS(delete_process(child_proc));
    TEST_SUCCESS(delete_process(proc));

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
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *threads[10];
    const size_t NUM_THREADS = sizeof(threads) / sizeof(threads[0]);

    for (size_t i = 0; i < NUM_THREADS; i++) {
        thread_t *thr = proc_new_thread(proc, (uintptr_t)fake_entry, 0, 0, 0);
        TEST_TRUE(thr != NULL);
        threads[i] = thr;
    }

    TEST_SUCCESS(delete_process(proc));
    TEST_SUCCEED();
}

// Now test handle copying/cleanup.
// I have made a mock handle state which can fail during copies/deletes when necessary.
// This should help confirm error paths work correctly for handle state errors.

typedef struct _mock_handle_state {
    handle_state_t super;

    allocator_t *al;

    bool ok_to_copy;
    bool ok_to_delete;
} mock_handle_state_t;

static fernos_error_t copy_mock_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_mock_handle_state(handle_state_t *hs);

static const handle_state_impl_t MOCK_HS_IMPL = {
    .copy_handle_state = copy_mock_handle_state,
    .delete_handle_state = delete_mock_handle_state
};

static handle_state_t *new_mock_handle_state(allocator_t *al, process_t *proc, handle_t h, bool otc, bool otd) {
    mock_handle_state_t *mhs = al_malloc(al, sizeof(mock_handle_state_t));
    if (!mhs) {
        return NULL;
    }

    // I think for such a simple handle, having the kernel and process be NULL
    // (just for this test) is OK.
    init_base_handle((handle_state_t *)mhs, &MOCK_HS_IMPL, NULL, proc, h, false);

    mhs->al = al;
    mhs->ok_to_copy = otc;
    mhs->ok_to_delete = otd;

    return (handle_state_t *)mhs;
}

static handle_state_t *new_da_mock_handle_state(process_t *proc, handle_t h, bool otc, bool otd) {
    return new_mock_handle_state(get_default_allocator(), proc, h, otc, otd);
}

static fernos_error_t copy_mock_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    mock_handle_state_t *mhs = (mock_handle_state_t *)hs;
    if (!(mhs->ok_to_copy)) {
        return FOS_E_UNKNWON_ERROR;
    }

    handle_state_t *mhs_copy = new_mock_handle_state(mhs->al, proc, mhs->super.handle, mhs->ok_to_copy, mhs->ok_to_delete);
    if (!mhs_copy) {
        return FOS_E_NO_MEM;
    }

    *out = mhs_copy;

    return FOS_E_SUCCESS;
}

static fernos_error_t delete_mock_handle_state(handle_state_t *hs) {
    mock_handle_state_t *mhs = (mock_handle_state_t *)hs;
    if (!(mhs->ok_to_delete)) {
        return FOS_E_ABORT_SYSTEM;
    }

    al_free(mhs->al, mhs);

    return FOS_E_SUCCESS;
}

static bool test_fork_with_handles(void) {
    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    process_t *proc = new_da_process(0, pd, NULL);
    TEST_TRUE(proc != NULL);

    thread_t *thr = proc_new_thread(proc, 0x45, 0, 0, 0);
    TEST_TRUE(thr != NULL);

    handle_t h0 = idtb_pop_id(proc->handle_table);
    TEST_TRUE(h0 != idtb_null_id(proc->handle_table));
    handle_state_t *hs0 = new_da_mock_handle_state(proc, h0, true, true);
    TEST_TRUE(hs0 != NULL);
    idtb_set(proc->handle_table, h0, hs0);

    handle_t h1 = idtb_pop_id(proc->handle_table);
    TEST_TRUE(h1 != idtb_null_id(proc->handle_table));
    handle_state_t *hs1 = new_da_mock_handle_state(proc, h1, true, true);
    TEST_TRUE(hs1 != NULL);
    idtb_set(proc->handle_table, h1, hs1);

    process_t *child;
    TEST_SUCCESS(new_process_fork(proc, proc->main_thread, 10, &child));

    TEST_TRUE(idtb_get(child->handle_table, h0) != NULL);
    TEST_TRUE(idtb_get(child->handle_table, h0) != hs0);
    TEST_TRUE(idtb_get(child->handle_table, h1) != NULL);
    TEST_TRUE(idtb_get(child->handle_table, h1) != hs1);

    // These few lines are pretty important. The confirm that a non catastrophic failure to copy
    // properly exits `new_process_fork` without memory leaks.
    process_t *bad_child;
    ((mock_handle_state_t *)hs0)->ok_to_copy = false;
    TEST_FAILURE(new_process_fork(proc, proc->main_thread, 10, &bad_child));

    // Uncomment these lines to make sure the destructor propegates errors correctly!
    //((mock_handle_state_t *)hs0)->ok_to_delete = false;
    //TEST_FAILURE(delete_process(proc));
    //while (1);

    TEST_SUCCESS(delete_process(child));
    TEST_SUCCESS(delete_process(proc));
    TEST_SUCCEED();
}

static bool test_proc_exec(void) {
    phys_addr_t pd = pop_initial_user_pd_copy();
    process_t *mock_parent = (process_t *)0x1;

    process_t *proc = new_da_process(0, pd, mock_parent);
    TEST_TRUE(proc != NULL);

    // Let's give this process some handles and threads.

    // Main thread should be preserved!
    thread_t *mt = proc_new_thread(proc, (uint32_t)fake_entry, 0, 0, 0);
    TEST_TRUE(mt != NULL);

    // This thread will be deleted!
    TEST_TRUE(proc_new_thread(proc, (uint32_t)fake_entry, 0, 0, 0) != NULL);

    // Finally, some handles too?

    const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);
    handle_t handles[3];
    const size_t handles_len = sizeof(handles) / sizeof(handles[0]);

    for (size_t i = 0; i < handles_len; i++) {
        handle_t h = idtb_pop_id(proc->handle_table);
        TEST_TRUE(h != NULL_HANDLE);

        handle_state_t *hs = new_da_mock_handle_state(proc, h, true, true);
        TEST_TRUE(hs != NULL);

        idtb_set(proc->handle_table, h, hs);

        handles[i] = h;
    }

    proc->in_handle = NULL_HANDLE;
    proc->out_handle = handles[0]; // Only 0 should be preserved!

    // Make a new page directory!
    phys_addr_t new_pd = pop_initial_user_pd_copy();
    TEST_SUCCESS(proc_exec(proc, new_pd, (uint32_t)fake_entry, 0, 0, 0));

    // Confirm page directories were switched.
    TEST_EQUAL_HEX(new_pd, proc->pd);
    
    // Confirm main thread persisted.
    TEST_EQUAL_HEX(mt, proc->main_thread);

    // Confirm only the main thread still exists!
    for (thread_id_t tid = 0; tid < FOS_MAX_THREADS_PER_PROC; tid++) {
        if (tid == mt->tid) {
            TEST_EQUAL_HEX(mt, idtb_get(proc->thread_table, tid));
        } else {
            TEST_EQUAL_HEX(NULL, idtb_get(proc->thread_table, tid));
        }
    }

    // Now confirm just the out handle exists!
    for (handle_t h = 0; h < FOS_MAX_HANDLES_PER_PROC; h++) {
        if (h == proc->out_handle) {
            TEST_TRUE(idtb_get(proc->handle_table, h) != NULL);
        } else {
            TEST_EQUAL_HEX(NULL, idtb_get(proc->handle_table, h));
        }
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
    RUN_TEST(test_complex_process);
    RUN_TEST(test_fork_with_handles);
    RUN_TEST(test_proc_exec);
    return END_SUITE();
}
