
#pragma once

#include "k_startup/plugin.h"

typedef struct _plugin_shm_t plugin_shm_t;

struct _plugin_shm_t {
    uint8_t dummy;
};

plugin_t *new_plugin_shm(kernel_state_t *ks);
