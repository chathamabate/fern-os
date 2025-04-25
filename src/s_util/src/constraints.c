
#include "s_util/constraints.h"
#include "s_util/err.h"
#include "s_util/misc.h"

fernos_error_t validate_constraints(void) {
    CHECK_ALIGN(FOS_CODE_AND_DATA_AREA_END, M_4K);

    CHECK_ALIGN(FOS_FREE_AREA_START, M_4K);
    CHECK_ALIGN(FOS_FREE_AREA_END, M_4K);

    CHECK_ALIGN(FOS_STAC

    if (FOS_FREE_AREA_START < FOS_CODE_AND_DATA_AREA_END) {
        return FOS_INVALID_RANGE;
    }

    if (FOS_FREE_AREA_END < FOS_FREE_AREA_START) {

    }

    return FOS_SUCCESS;
}
