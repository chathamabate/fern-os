
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "s_data/id_table.h"


struct _schedule_node_t {
    schedule_node_t *next;
    schedule_node_t *prev;

    thread_t *thr;
};

struct _kernel_state_t {
    schedule_node_t *schedule_first;
    schedule_node_t *schedule_last;
    // oogh... 
    // It's kinda hard to figure out this schedule thing...
    // Should each thread itself be part of 

    id_table_t *proc_table; // All processes have a globally unique id.
    
    process_t *root_proc;
};


