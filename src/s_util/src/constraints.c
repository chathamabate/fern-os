
#include "s_util/constraints.h"
#include "s_util/err.h"
#include "s_util/misc.h"

/**
 * From the linker script.
 */
extern uint8_t _static_area_end[];

fernos_error_t validate_constraints(void) {
    CHECK_ALIGN(FOS_CODE_AND_DATA_AREA_END, M_4K);

    // These alignment checks are slightly redundant, but whatevs.

    CHECK_ALIGN(FOS_FREE_AREA_START, M_4K);
    CHECK_ALIGN(FOS_FREE_AREA_END, M_4K);
    CHECK_ALIGN(FOS_FREE_AREA_SIZE, M_4K);

    CHECK_ALIGN(FOS_KERNEL_STACK_SIZE, M_4K);
    CHECK_ALIGN(FOS_KERNEL_STACK_START, M_4K);
    CHECK_ALIGN(FOS_KERNEL_STACK_END, M_4K);

    CHECK_ALIGN(FOS_THREAD_STACK_SIZE, M_4K);
    CHECK_ALIGN(FOS_STACK_AREA_SIZE, M_4K);
    CHECK_ALIGN(FOS_STACK_AREA_END, M_4K);
    CHECK_ALIGN(FOS_STACK_AREA_START, M_4K);
    CHECK_ALIGN(FOS_STACK_AREA_MAX_SIZE, M_4K);

    // All stack sizes must be at least 2 pages.
    // (Since all stacks require at least one redzone page!)

    if (FOS_KERNEL_STACK_SIZE < 2 * M_4K) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_THREAD_STACK_SIZE < 2 * M_4K) {
        return FOS_INVALID_RANGE;
    }

    if ((uint32_t)_static_area_end > FOS_CODE_AND_DATA_AREA_END) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_FREE_AREA_START < FOS_CODE_AND_DATA_AREA_END) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_FREE_AREA_END < FOS_FREE_AREA_START) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_STACK_AREA_START < FOS_FREE_AREA_END) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_STACK_AREA_SIZE > FOS_STACK_AREA_MAX_SIZE) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_MAX_THREADS_PER_PROC > 32) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_MAX_PLUGINS > 256) {
        return FOS_INVALID_RANGE;
    }

    // If we make it here, the OS should be safe to boot!

    return FOS_SUCCESS;
}
