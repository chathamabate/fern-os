
#pragma once

#include "plugin.h"

typedef struct _plugin_kb_t plugin_kb_t;

struct _plugin_kb_t {
    plugin_t super;
};

plugin_t *new_plugin_kb(kernel_state_t *ks);


