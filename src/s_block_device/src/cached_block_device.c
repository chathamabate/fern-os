
#include "s_block_device/block_device.h"
#include "s_block_device/cached_block_device.h"
#include "s_util/str.h"

static size_t cbd_num_sectors(block_device_t *bd);
static size_t cbd_sector_size(block_device_t *bd);
static fernos_error_t cbd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest);
static fernos_error_t cbd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src);
static fernos_error_t cbd_read_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest);
static fernos_error_t cbd_write_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src);
static fernos_error_t cbd_flush(block_device_t *bd);
static void delete_cached_block_device(block_device_t *bd);

static const block_device_impl_t CBD_IMPL = {
    .bd_num_sectors = cbd_num_sectors,
    .bd_sector_size = cbd_sector_size,
    .bd_read = cbd_read,
    .bd_write = cbd_write,

    .bd_read_piece = cbd_read_piece,
    .bd_write_piece = cbd_write_piece,

    .bd_flush = cbd_flush,
    .bd_dump = NULL,
    .delete_block_device = delete_cached_block_device
};

static bool size_t_eq_f(chained_hash_map_t *chm, const void *k0, const void *k1) {
    (void)chm;
    return *(const size_t *)k0 == *(const size_t *)k1;
}

static uint32_t size_t_hash_f(chained_hash_map_t *chm, const void *k) {
    (void)chm;
    // Kinda arbitrary hash function.
    return (((((*(const size_t *)k) * 3) + 2) * 17) + 1) * 13;
}

block_device_t *new_cached_block_device(allocator_t *al, block_device_t *bd, size_t cc, uint32_t seed) {
    if (!al || !bd || cc == 0) {
        return NULL;
    }

    const size_t ss = bd_sector_size(bd);

    cached_block_device_t *cbd = al_malloc(al, sizeof(cached_block_device_t));


    bool cache_alloc_fail = false;
    cached_sector_entry_t *c = al_malloc(al, sizeof(cached_sector_entry_t) * cc);

    if (c) {
        for (size_t i = 0; i < cc; i++) {
            if (cache_alloc_fail) {
                c[i].sector = NULL;
            } else {
                c[i].sector = al_malloc(al, ss);

                if (!(c[i].sector)) {
                    cache_alloc_fail = true;
                }
            }
        }
    }

    map_t *sm = new_chained_hash_map(al, sizeof(size_t), sizeof(size_t), 3, size_t_eq_f, size_t_hash_f);

    if (!cbd || !c || cache_alloc_fail || !sm) {
        delete_map(sm);

        if (c) {
            for (size_t i = 0; i < cc; i++) {
                al_free(al, c[i].sector);
            }
        }

        al_free(al, c);
        al_free(al, cbd);
        
        return NULL;
    }

    // Total Success!

    *(const block_device_impl_t **)&(cbd->super.impl) = &CBD_IMPL;
    *(allocator_t **)&(cbd->al) = al;
    *(block_device_t **)&(cbd->wrapped_bd) = bd;
    cbd->r = rand(seed);
    *(size_t *)&(cbd->cache_cap) = cc;
    cbd->cache_fill = 0;
    *(cached_sector_entry_t **)&(cbd->cache) = c;
    *(map_t **)&(cbd->sector_map) = sm;

    return (block_device_t *)cbd;
}

static size_t cbd_num_sectors(block_device_t *bd) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;
    return bd_num_sectors(cbd->wrapped_bd);
}

static size_t cbd_sector_size(block_device_t *bd) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;
    return bd_sector_size(cbd->wrapped_bd);
}

static fernos_error_t cbd_read(block_device_t *bd, size_t sector_ind, size_t num_sectors, void *dest) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    if (!dest) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(cbd->wrapped_bd);
    const size_t ss = bd_sector_size(cbd->wrapped_bd);

    if (sector_ind >= ns || sector_ind + num_sectors > ns || sector_ind + num_sectors < sector_ind) {
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err;

    // We want to pass large ranges directly to the wrapped block device.
    
    // This is the index of the of the first sector which is yet to be read out.
    size_t section_start = sector_ind;

    for (size_t i = sector_ind; i < sector_ind + num_sectors; i++) {
        size_t *cache_entry_ind_p = mp_get(cbd->sector_map, &i);

        if (cache_entry_ind_p) {
            // Ok, we have a cache hit, let's write read out everything we've seen so far.

            // Is there a sector (or group of sectors) before this one which is yet to be read out.
            // It is gauranteed that such sectors are not in the cache!
            if (section_start < i) {
                err = bd_read(cbd->wrapped_bd, section_start, i - section_start, 
                        (uint8_t *)dest + ((section_start - sector_ind) * ss));

                if (err != FOS_SUCCESS) {
                    return err;
                }
            }

            size_t cache_entry_ind = *cache_entry_ind_p;

            void *cached_sector = cbd->cache[cache_entry_ind].sector;
            mem_cpy((uint8_t *)dest + ((i - sector_ind) * ss), cached_sector, ss);

            section_start = i + 1;
        }
    }

    if (section_start < sector_ind + num_sectors) {
        err = bd_read(cbd->wrapped_bd, section_start, 
                sector_ind + num_sectors - section_start, 
                (uint8_t *)dest + ((section_start - sector_ind) * ss));

        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    return FOS_SUCCESS;
}

static fernos_error_t cbd_write(block_device_t *bd, size_t sector_ind, size_t num_sectors, const void *src) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(cbd->wrapped_bd);
    const size_t ss = bd_sector_size(cbd->wrapped_bd);

    if (sector_ind >= ns || sector_ind + num_sectors > ns || sector_ind + num_sectors < sector_ind) {
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err;

    // Index of the first sector NOT to be written out yet.
    size_t section_start = sector_ind;

    for (size_t i = sector_ind; i < sector_ind + num_sectors; i++) {
        size_t *cache_entry_ind_p = mp_get(cbd->sector_map, &i);

        if (cache_entry_ind_p) {

            // A group of sectors which all missed the cache will be written directly to the 
            // wrapped bd.
            if (section_start < i) {
                err = bd_write(cbd->wrapped_bd, section_start, i - section_start, 
                        (const uint8_t *)src + ((section_start - sector_ind) * ss)); 

                if (err != FOS_SUCCESS)  {
                    return err;
                }
            }

            size_t cache_entry_ind = *cache_entry_ind_p;
            void *cache_sector = cbd->cache[cache_entry_ind].sector;
            
            mem_cpy(cache_sector, (const uint8_t *)src + ((i - sector_ind) * ss), ss);

            section_start = i + 1;
        }
    }

    if (section_start < sector_ind + num_sectors) {
        err = bd_write(cbd->wrapped_bd, section_start, sector_ind + num_sectors - section_start, 
                (const uint8_t *)src + ((section_start - sector_ind) * ss));

        if (err != FOS_SUCCESS) {
            return err;
        } 
    }

    return FOS_SUCCESS;
}

/**
 * No parameter checks, this is an internal helper which writes out the cache entry index of a given
 * sector.
 *
 * If the sector is not in the cache, it will be read into the cache. 
 * (Potentially overwriting some other cache entry)
 */
static fernos_error_t cbd_get_cache_entry_ind(cached_block_device_t *cbd, size_t sector_ind, size_t *cache_entry_ind_p) {
    fernos_error_t err;

    size_t *ind_p = mp_get(cbd->sector_map, &sector_ind);
    size_t ind;

    if (!ind_p) {
        // Cache Miss!  (Let's load a sector into the cache!)

        if (cbd->cache_fill < cbd->cache_cap) {
            // Cache is not full.
            ind = cbd->cache_fill++;

        } else {
            // Cache is full. (Remove a used sector)
            
            ind = next_rand(&(cbd->r)) % cbd->cache_cap;
            
            // Write out sector which is to be written over.
            err = bd_write(cbd->wrapped_bd, cbd->cache[ind].sector_ind, 1, cbd->cache[ind].sector);
            if (err != FOS_SUCCESS) {
                return err;
            }

            // Remove sector from mapping.
            mp_remove(cbd->sector_map, &(cbd->cache[ind].sector_ind));
        }

        cbd->cache[ind].sector_ind = sector_ind;

        err = mp_put(cbd->sector_map, &sector_ind, &ind);
        if (err != FOS_SUCCESS) {
            return err; 
        }

        err = bd_read(cbd->wrapped_bd, sector_ind, 1, cbd->cache[ind].sector); 
        if (err != FOS_SUCCESS) {
            return err; 
        }
    } else {
        ind = *ind_p;
    }

    *cache_entry_ind_p = ind;

    return FOS_SUCCESS;
}

static fernos_error_t cbd_read_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, void *dest) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    if (!dest) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(cbd->wrapped_bd);
    const size_t ss = bd_sector_size(cbd->wrapped_bd);

    if (sector_ind >= ns || offset >= ss || offset + len > ss || offset + len < offset) {
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err;

    size_t cache_entry_ind;
    err = cbd_get_cache_entry_ind(cbd, sector_ind, &cache_entry_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Copy from cache into the dest buffer.

    uint8_t *cache_sector = cbd->cache[cache_entry_ind].sector;
    mem_cpy(dest, cache_sector + offset, len);

    return FOS_SUCCESS;
}

static fernos_error_t cbd_write_piece(block_device_t *bd, size_t sector_ind, size_t offset, size_t len, const void *src) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    const size_t ns = bd_num_sectors(cbd->wrapped_bd);
    const size_t ss = bd_sector_size(cbd->wrapped_bd);

    if (sector_ind >= ns || offset >= ss || offset + len > ss || offset + len < offset) {
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err;

    size_t cache_entry_ind;
    err = cbd_get_cache_entry_ind(cbd, sector_ind, &cache_entry_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Copy from cache into the dest buffer.

    uint8_t *cache_sector = cbd->cache[cache_entry_ind].sector;
    mem_cpy(cache_sector + offset, src, len);

    return FOS_SUCCESS;
}

static fernos_error_t cbd_flush(block_device_t *bd) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    // First thing we do is write out all the cached sectors.

    fernos_error_t err;

    const size_t *sector_ind;
    size_t *cache_entry_ind;

    mp_reset_iter(cbd->sector_map);
    for (err = mp_get_iter(cbd->sector_map, (const void **)&sector_ind, (void **)&cache_entry_ind);
            err == FOS_SUCCESS; err = mp_next_iter(cbd->sector_map, (const void **)&sector_ind, (void **)&cache_entry_ind)) {
        err = bd_write(cbd->wrapped_bd, *sector_ind, 1, 
                cbd->cache[*cache_entry_ind].sector);  

        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    if (err != FOS_EMPTY) {
        return err;
    }

    // This could be done better if I implemented a mp_clear function.
    //
    // The idea is if we just empty the sector map, the cache appears empty.
    // I just delete and recreate the map here.

    delete_map(cbd->sector_map);    

    *(map_t **)&(cbd->sector_map) = new_chained_hash_map(cbd->al, 
            sizeof(size_t), sizeof(size_t), 3, 
            size_t_eq_f, size_t_hash_f);

    if (!(cbd->sector_map)) {
        return FOS_NO_MEM;
    }

    cbd->cache_fill = 0;

    return FOS_SUCCESS;
}

static void delete_cached_block_device(block_device_t *bd) {
    cached_block_device_t *cbd = (cached_block_device_t *)bd;

    delete_map(cbd->sector_map);

    for (size_t i = 0; i < cbd->cache_cap; i++) {
        al_free(cbd->al, cbd->cache[i].sector);
    }

    al_free(cbd->al, cbd->cache);

    al_free(cbd->al, cbd);
}

