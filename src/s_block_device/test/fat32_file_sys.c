
#include "s_block_device/test/fat32_file_sys.h"
#include "s_block_device/test/file_sys.h"
#include "s_block_device/file_sys.h"
#include "s_block_device/fat32_file_sys.h"
#include "s_util/datetime.h"
#include "s_block_device/block_device.h"
#include "s_block_device/mem_block_device.h"

static void now(fernos_datetime_t *dt) {
    // The date time features are optional for the generic FS interface.
    // My FAT32 implementation does use them, however, this test suite will
    // not check at all.
    *dt = (fernos_datetime_t) { 0 };
}

static file_sys_t *gen_fat32_fs(void) {
    fernos_error_t err;

    block_device_t *bd = new_da_mem_block_device(FAT32_REQ_SECTOR_SIZE, 1024);

    if (bd) {
        err = init_fat32(bd, 0, bd_num_sectors(bd), 4);

        file_sys_t *fs;

        if (err == FOS_SUCCESS) {
            err = parse_new_da_fat32_file_sys(bd, 0, 0, true, now, &fs);
        }

        if (err == FOS_SUCCESS) {
            return fs; // Success!
        }
    }

    // Failure.
    delete_block_device(bd);
    return NULL;
}

bool test_fat32_file_sys(void) {
    return test_file_sys("FAT32 File Sys", gen_fat32_fs);
}
