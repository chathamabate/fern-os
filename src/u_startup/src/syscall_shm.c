
#include "u_startup/syscall.h"
#include "u_startup/syscall_shm.h"

fernos_error_t sc_shm_new_semaphore(sem_id_t *sem, uint32_t num_passes) {
    return sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_NEW_SEM, (uint32_t)sem, num_passes, 0, 0);
}

fernos_error_t sc_shm_sem_dec(sem_id_t sem) {
    return sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_DEC, (uint32_t)sem, 0, 0, 0);
}

void sc_shm_sem_inc(sem_id_t sem) {
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_INC, (uint32_t)sem, 0, 0, 0);
}

void sc_shm_close_semaphore(sem_id_t sem) {
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_CLOSE, (uint32_t)sem, 0, 0, 0);
}
