
#pragma once

#include "k_sys/page.h"
#include "os_defs.h"
#include "s_util/misc.h"

#define NUM_IDENTITY_PTS        (IDENTITY_AREA_SIZE / M_4M)
#define NUM_IDENTITY_PT_ENTRIES (IDENTITY_AREA_SIZE / M_4K)

// NOTE: The first entry of the first page table will always be marked non-present.
// This way, use of a NULL pointer results in a fault.
extern pt_entry_t identity_pts[NUM_IDENTITY_PTS][1024];

extern pt_entry_t kernel_pd[1024];


// Returns 0 on success, 1 on Failure.
//int init_paging(void);

