
#pragma once

#include "s_gfx/window.h"

typedef struct _window_composite_t window_composite_t;
typedef struct _window_composite_impl_t window_composite_impl_t;

/**
 * A Composite Window is a window which can dynamically contain other "sub windows".
 *
 * The intention of this type was to lay the ground work for desktop environments. A desktop
 * environment will be a window itself! 
 */
struct _window_composite_t {
    window_t super;

    const window_composite_impl_t *impl;
};

void init_window_composite_base(window_composite_t *wc, 
        gfx_buffer_t *buf, const window_impl_t *super_impl, const window_composite_impl_t *impl);

void deinit_window_composite_base(window_composite_t *wc);

/**
 * Give the composite window a sub window.
 * The sub window remains "registered" until explicitly "deregsitered" with the below function.
 * Even if the sub window goes into a fatal error state, it should stay regsitered.
 *
 * FOS_E_SUCCESS if the sub window was successfully given to the composite window.
 * FOS_E_FATAL if the composite window failed catastrophically during this call. (Can no longer
 * be used)
 * Any other error indicates the sub window couldn't be registered.
 */
fernos_error_t wc_register_sub_window(window_composite_t *wc, window_t *sw);

/**
 * Explicitly deregister a sub window from a composite window.
 *
 * Should never fail!
 */
static inline void wc_deregister_sub_window(window_composite_t *wc, window_t *sw);
