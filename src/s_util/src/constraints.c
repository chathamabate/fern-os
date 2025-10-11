
#include "s_util/constraints.h"
#include "s_util/err.h"
#include "s_util/misc.h"

/**
 * Is aligned macro which works with the preprocessor
 */
#define IS_ALIGNED_PP(val, align) (((val) & (align - 1)) == 0)

#define IS_VALID_AREA(start, end) (IS_ALIGNED_PP(start, M_4K) && IS_ALIGNED_PP(end, M_4K) && ((start) < (end)))

// Check alignments and borders of all areas.

#if !IS_VALID_AREA(FOS_AREA_START, FOS_AREA_END)
#pragma message("AREA (Start: " STRINGIFY(FOS_AREA_START) ", END: " STRINGIFY(FOS_AREA_END) ")")
#error "AREA is invalid!"
#endif

#if !IS_VALID_AREA(FOS_KERNEL_AREA_START, FOS_KERNEL_AREA_END)
#pragma message("KERNEL_AREA (Start: " STRINGIFY(FOS_KERNEL_AREA_START) ", END: " STRINGIFY(FOS_KERNEL_AREA_END) ")")
#error "KERNEL_AREA is invalid!"
#endif

#if !IS_VALID_AREA(FOS_APP_AREA_START, FOS_APP_AREA_END)
#pragma message("APP_AREA (Start: " STRINGIFY(FOS_APP_AREA_START) ", END: " STRINGIFY(FOS_APP_AREA_END) ")")
#error "APP_AREA is invalid!"
#endif

#if !IS_VALID_AREA(FOS_FREE_AREA_START, FOS_FREE_AREA_END)
#pragma message("FREE_AREA (Start: " STRINGIFY(FOS_FREE_AREA_START) ", END: " STRINGIFY(FOS_FREE_AREA_END) ")")
#error "FREE_AREA is invalid!"
#endif

#if !IS_VALID_AREA(FOS_STACK_AREA_START, FOS_STACK_AREA_END)
#pragma message("STACK_AREA (Start: " STRINGIFY(FOS_STACK_AREA_START) ", END: " STRINGIFY(FOS_STACK_AREA_END) ")")
#error "STACK_AREA is invalid!"
#endif

// Remember, the kernel stack is INSIDE the STACK_AREA.

#if !IS_VALID_AREA(FOS_KERNEL_STACK_START, FOS_KERNEL_STACK_END)
#pragma message("KERNEL_STACK (Start: " STRINGIFY(FOS_KERNEL_STACK_START) ", END: " STRINGIFY(FOS_KERNEL_STACK_END) ")")
#error "KERNEL_STACK is invalid!"
#endif

#if  FOS_KERNEL_STACK_SIZE < (2 * M_4K)
#error "Kernel stack size must be at least 2 pages!"
#endif

#if FOS_THREAD_STACK_SIZE < (2 * M_4K)
#error "Thread stack size must be at least 2 pages!"
#endif

#if FOS_MAX_THREADS_PER_PROC == 0 || FOS_MAX_THREADS_PER_PROC > 32
#error "Invalid value for FOS_MAX_THREADS_PER_PROC!"
#endif

#if !IS_ALIGNED_PP(FOS_THREAD_STACK_SIZE, M_4K)
#error "FOS_THREAD_STACK_SIZE must be 4K aligned!"
#endif

#if FOS_STACK_AREA_SIZE < FOS_MIN_STACK_AREA_SIZE
#error "FOS_STACK_AREA_SIZE is too small given constraints!"
#endif

// Check for overlaps!!!

#if FOS_KERNEL_AREA_START < FOS_AREA_START
#error "KERNEL_AREA starts too early!"
#endif

#if FOS_KERNEL_AREA_END > FOS_APP_AREA_START
#error "KERNEL_AREA overlaps APP_AREA!"
#endif

#if FOS_APP_AREA_END > FOS_FREE_AREA_START
#error "APP_AREA overlaps FREE_AREA!"
#endif

#if FOS_FREE_AREA_END > FOS_STACK_AREA_START
#error "FREE_AREA overlaps STACK_AREA!"
#endif

#if FOS_STACK_AREA_END > FOS_AREA_END
#error "STACK_AREA exceeds AREA!"
#endif

// Handle and plugin stuff.

#if FOS_MAX_HANDLES_PER_PROC == 0 || FOS_MAX_HANDLES_PER_PROC > 256
#error "Invalid value for FOS_MAX_HANDLES_PER_PROC!"
#endif

#if FOS_MAX_PLUGINS == 0 || FOS_MAX_PLUGINS > 256
#error "Invalid value for FOS_MAX_PLUGINS!"
#endif

fernos_error_t validate_constraints(void) {
    // This used to do something, but then I learned about preprocessor errors.
    // Leaving this in here so that there is something to compile.

    return FOS_E_SUCCESS;
}
