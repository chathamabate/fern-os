
#include "u_startup/syscall.h"
#include "u_startup/syscall_shm.h"

fernos_error_t sc_shm_alloc(void **out, size_t bytes) {
    return sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_ALLOC, (uint32_t)out, (uint32_t)bytes, 0, 0);
}

void sc_shm_release(void *shm) {
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_RELEASE, (uint32_t)shm, 0, 0, 0);
}
