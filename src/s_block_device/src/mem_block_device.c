
#include "s_block_device/mem_block_device.h"
#include "s_util/str.h"

static size_t mbd_num_sectors(block_device_t *bd);
static size_t mbd_sector_size(block_device_t *bd);
static fernos_error_t mbd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest);
static fernos_error_t mbd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src);
static fernos_error_t mbd_read_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest);
static fernos_error_t mbd_write_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src);
static void delete_mem_block_device(block_device_t *bd);

static const block_device_impl_t MBD_IMPL = {
    .bd_num_sectors = mbd_num_sectors,
    .bd_sector_size = mbd_sector_size,
    .bd_read = mbd_read,
    .bd_write = mbd_write,

    .bd_read_piece = mbd_read_piece,
    .bd_write_piece = mbd_write_piece,

    .bd_flush = NULL,
    .bd_dump = NULL,
    .delete_block_device = delete_mem_block_device
};

block_device_t *new_mem_block_device(allocator_t *al, size_t ss, size_t ns) {
    if (!al || ss == 0 || ns == 0) {
        return NULL;
    }

    mem_block_device_t *mbd = al_malloc(al, sizeof(mem_block_device_t));
    void *mem = al_malloc(al, ss * ns);
    if (!mbd || !mem) {
        al_free(al, mbd);
        al_free(al, mem);

        return NULL;
    }
    
    *(const block_device_impl_t **)&(mbd->super.impl) = &MBD_IMPL;
    *(allocator_t **)&(mbd->al) = al;
    *(size_t *)&(mbd->sector_size) = ss;
    *(size_t *)&(mbd->num_sectors) = ns;
    *(void **)&(mbd->mem) = mem;

    return (block_device_t *)mbd;
}

static size_t mbd_num_sectors(block_device_t *bd) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    return mbd->num_sectors;
}

static size_t mbd_sector_size(block_device_t *bd) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    return mbd->sector_size;
}

static fernos_error_t mbd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    if (!dest) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= mbd->num_sectors || sector_ind + num_sectors > mbd->num_sectors 
            || sector_ind + num_sectors < sector_ind) {
        return FOS_INVALID_RANGE;
    }

    if (num_sectors > 0) {
        mem_cpy(dest, 
                (uint8_t *)(mbd->mem) + (sector_ind * mbd->sector_size),
                num_sectors * mbd->sector_size);

    }

    return FOS_SUCCESS;
}

static fernos_error_t mbd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= mbd->num_sectors || sector_ind + num_sectors > mbd->num_sectors || 
            sector_ind + num_sectors < sector_ind) {
        return FOS_INVALID_RANGE;
    }

    if (num_sectors > 0) {
        mem_cpy((uint8_t *)(mbd->mem) + (sector_ind * mbd->sector_size),
                src,
                num_sectors * mbd->sector_size);

    }

    return FOS_SUCCESS;
}

static fernos_error_t mbd_read_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    if (!dest) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= mbd->num_sectors || offset >= mbd->sector_size || offset + len > mbd->sector_size ||
            offset + len < offset) {
        return FOS_INVALID_RANGE;
    }

    if (len > 0) {
        mem_cpy(dest, (uint8_t *)(mbd->mem) + (sector_ind * mbd->sector_size) + offset, len);
    }

    return FOS_SUCCESS;
}

static fernos_error_t mbd_write_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    if (sector_ind >= mbd->num_sectors || offset >= mbd->sector_size || offset + len > mbd->sector_size ||
            offset + len < offset) {
        return FOS_INVALID_RANGE;
    }

    if (len > 0) {
        mem_cpy((uint8_t *)(mbd->mem) + (sector_ind * mbd->sector_size) + offset, src, len);
    }

    return FOS_SUCCESS;

}

static void delete_mem_block_device(block_device_t *bd) {
    mem_block_device_t *mbd = (mem_block_device_t *)bd;

    al_free(mbd->al, mbd->mem);
    al_free(mbd->al, mbd);
}

