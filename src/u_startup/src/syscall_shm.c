
#include "s_bridge/shared_defs.h"
#include "u_startup/syscall.h"
#include "u_startup/syscall_shm.h"

fernos_error_t sc_shm_new_semaphore(sem_id_t *sem, uint32_t num_passes) {
    return sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_NEW_SEM, (uint32_t)sem, num_passes, 0, 0);
}

/*
 * I add in memory barriers here. They aren't really necessary given we are using
 * a single core, but whatever!
 */

fernos_error_t sc_shm_sem_dec(sem_id_t sem) {
    fernos_error_t err = sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_DEC, (uint32_t)sem, 0, 0, 0);
    __sync_synchronize();

    return err; 
}

void sc_shm_sem_inc(sem_id_t sem) {
    __sync_synchronize();
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_INC, (uint32_t)sem, 0, 0, 0);
}

void sc_shm_close_semaphore(sem_id_t sem) {
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_SEM_CLOSE, (uint32_t)sem, 0, 0, 0);
}

fernos_error_t sc_shm_new_shm(size_t bytes, void **shm) {
    return sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_NEW_SHM, (uint32_t)bytes, (uint32_t)shm, 0, 0);
}

void sc_shm_close_shm(void *shm) {
    (void)sc_plg_cmd(PLG_SHARED_MEM_ID, PLG_SHM_PCID_CLOSE_SHM, (uint32_t)shm, 0, 0, 0);
}
