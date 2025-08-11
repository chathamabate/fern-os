
#include "s_block_device/fat32_dir.h"
#include "s_util/str.h"

/*
 * The directory functions below are slightly looser with cluster error checking than the 
 * above functions. This is because functions below rely on the functions above.
 *
 * I tried to make it so that if a bad cluster is reached, the above functions fail.
 * This way, the below functions don't need to check for bad clusters.
 *
 * Overall, though, this is starting to get kinda messy. I am trying my best.
 */

fernos_error_t fat32_new_dir(fat32_device_t *dev, uint32_t *slot_ind) {
    if (!slot_ind) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    uint32_t head;
    err = fat32_pop_free_fat_slot(dev, &head);
    if (err != FOS_SUCCESS) {
        return err;
    }

    fat32_dir_entry_t term;
    term.raw[0] = FAT32_DIR_ENTRY_TERMINTAOR;

    // Write out the terminator.
    err = fat32_write_piece(dev, head, 0, 0, sizeof(fat32_dir_entry_t), &term);
    if (err != FOS_SUCCESS) {
        return err;
    }

    *slot_ind = head;
    return FOS_SUCCESS;
}

fernos_error_t fat32_read_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, fat32_dir_entry_t *entry) {
    if (!entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    // This is basically a compile time constant but whatevs.
    // 512 / 32 = 16
    const uint32_t entries_per_sector = FAT32_REQ_SECTOR_SIZE / sizeof(fat32_dir_entry_t);

    err = fat32_read_piece(dev, slot_ind, entry_offset / entries_per_sector, 
            (entry_offset % entries_per_sector) * sizeof(fat32_dir_entry_t), 
            sizeof(fat32_dir_entry_t), entry);

    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_write_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, const fat32_dir_entry_t *entry) {
    if (!entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    const uint32_t entries_per_sector = FAT32_REQ_SECTOR_SIZE / sizeof(fat32_dir_entry_t);

    err = fat32_write_piece(dev, slot_ind, entry_offset / entries_per_sector, 
            (entry_offset % entries_per_sector) * sizeof(fat32_dir_entry_t), 
            sizeof(fat32_dir_entry_t), entry);

    if (err != FOS_SUCCESS) {
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_get_free_seq(fat32_device_t *dev, uint32_t slot_ind, uint32_t seq_len,
        uint32_t *entry_offset) {
    if (!entry_offset) {
        return FOS_BAD_ARGS;
    }

    if (seq_len == 0) {
        *entry_offset = 0;
        return FOS_SUCCESS;
    }

    const uint32_t dir_entries_per_cluster = 
        (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    fernos_error_t err;

    // When true, we are in the middle of a free sequence.
    bool in_free_seq = false;

    uint32_t iter = slot_ind;
    uint32_t iter_chain_index = 0;

    uint32_t last_free_offset = 0;
    uint32_t last_free_chain_index = 0;
    uint32_t free_seq_len = 0;

    while (true) {
        fat32_dir_entry_t entry;

        for (uint32_t i = 0; i < dir_entries_per_cluster; i++) {
            err = fat32_read_dir_entry(dev, iter, i, &entry);
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            if (entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR) {
                // Here we hit the terminator before we found an adequate length sequence.
                // Let's just advance the terminator and keep going.
                //
                // This could be more efficient, but whatevs.

                // First advance the terminator if there is space.
                if (i < dir_entries_per_cluster - 1) {
                    err = fat32_write_dir_entry(dev, iter, i + 1, &entry);
                    if (err != FOS_SUCCESS) {
                        return FOS_UNKNWON_ERROR;
                    }
                }

                // Next Write over the current index with an unused entry.
                entry.raw[0] = FAT32_DIR_ENTRY_UNUSED;

                err = fat32_write_dir_entry(dev, iter, i, &entry);
                if (err != FOS_SUCCESS) {
                    return FOS_UNKNWON_ERROR;
                }
            }
            
            if (entry.raw[0] == FAT32_DIR_ENTRY_UNUSED) {
                // Unused entry found.
                if (!in_free_seq) {
                    last_free_offset = i;
                    last_free_chain_index = iter_chain_index;
                    free_seq_len = 0;

                    in_free_seq = true;
                } 

                free_seq_len++;

                if (free_seq_len == seq_len) {
                    *entry_offset = (last_free_chain_index * dir_entries_per_cluster) + last_free_offset;
                    return FOS_SUCCESS;
                }
            } else if (in_free_seq) {
                // Used + we were iterating over a free sequence!
                in_free_seq = false;
            }
        }

        // Ok, now we must go to the next cluster!

        uint32_t next_iter;
        err = fat32_get_fat_slot(dev, iter, &next_iter);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (FAT32_IS_EOC(next_iter)) {
            // We made it to the end of our chain without finding a match, let's just
            // expand the chain here.

            uint32_t new_cluster;
            err = fat32_pop_free_fat_slot(dev, &new_cluster);
            if (err != FOS_SUCCESS) {
                return err; // This is OK because pop_fat_slot also returns an 
                            // FOS_NO_SPACE error.
            }

            // Place the directory terminator at the very beginning of this new directory
            // cluster.
            entry.raw[0] = FAT32_DIR_ENTRY_TERMINTAOR;
            err = fat32_write_dir_entry(dev, new_cluster, 0, &entry);
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            // Finally, add the new cluster to our current chain!
            err = fat32_set_fat_slot(dev, iter, new_cluster);
            if (err != FOS_SUCCESS) {
                return FOS_UNKNWON_ERROR;
            }

            next_iter = new_cluster;
        }

        iter = next_iter;
        iter_chain_index++;
    }
}

fernos_error_t fat32_erase_seq(fat32_device_t *dev, uint32_t slot_ind, uint32_t entry_offset) {
    fernos_error_t err;

    const uint32_t dir_entries_per_cluster = (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) /
        sizeof(fat32_dir_entry_t);
    
    uint32_t slot_iter = slot_ind;

    while (true) {
        uint32_t start = 0;
        if (slot_iter == slot_ind) {
            start = entry_offset;
        }

        for (uint32_t i = start; i < dir_entries_per_cluster; i++) {
            fat32_dir_entry_t entry;

            err = fat32_read_dir_entry(dev, slot_iter, i, &entry);
            if (err != FOS_SUCCESS) {
                return err;
            }

            if (entry.raw[0] == FAT32_DIR_ENTRY_UNUSED || 
                    entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR) {
                return FOS_SUCCESS; // We made it to an explicit end!
                                    // This may imply a malformed chain was erased, but we'll allow
                                    // it.
            }

            bool sfn = !FT32F_ATTR_IS_LFN(entry.short_fn.attrs);

            entry.raw[0] = FAT32_DIR_ENTRY_UNUSED;
            err = fat32_write_dir_entry(dev, slot_iter, i, &entry);
            if (err != FOS_SUCCESS) {
                return err;
            }

            // We reached the end of a chain
            if (sfn) {
                return FOS_SUCCESS;
            }
        }

        uint32_t next_slot_iter;
        err = fat32_get_fat_slot(dev, slot_iter, &next_slot_iter);
        if (err != FOS_SUCCESS) {
            return err;
        }

        if (FAT32_IS_EOC(next_slot_iter) || next_slot_iter < 2 || next_slot_iter >= dev->num_fat_slots) {
            return FOS_SUCCESS; // Again, the chain could've ended.. or been some invalid value.
                                // We'll just return SUCCESS, we erased what we could.
        }

        slot_iter = next_slot_iter;
    }
}

fernos_error_t fat32_next_dir_seq(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, uint32_t *seq_start) {
    if (!seq_start) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    uint32_t iter;

    const uint32_t dir_entries_per_cluster = 
        (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    // Here we skip ahead until the cluster which holds the entry we are starting at.
    err = fat32_traverse_chain(dev, slot_ind, entry_offset / dir_entries_per_cluster, 
            &iter);
    if (err == FOS_INVALID_INDEX) {
        return FOS_EMPTY; // We overshot the end of the directory sectors.
    }

    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }
    
    uint32_t entries_checked = 0;
    bool first_iter = true;

    // For this function, we are not going to use the fwd_ind paradigm 
    // because a sequence of empty entries could potentially be very long.
    // It'd be more efficient to just iterate over the cluster chain directly ourselves.

    while (true) {
        fat32_dir_entry_t entry;

        uint32_t start = 0;

        if (first_iter) {
            start = entry_offset % dir_entries_per_cluster;
            first_iter = false;
        }

        for (uint32_t i = start; i < dir_entries_per_cluster; i++, entries_checked++) {
            err = fat32_read_dir_entry(dev, iter, i, &entry);
            if (err != FOS_SUCCESS) {
                return err;
            }

            if (entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR) {
                return FOS_EMPTY; // We hit the terminator.
            }

            if (entry.raw[0] != FAT32_DIR_ENTRY_UNUSED) {
                // We hit a used entry!!! WOOO!!!
                *seq_start = entry_offset + entries_checked;
                return FOS_SUCCESS;
            }
        }

        // Now we need to advance cluster iterator.

        uint32_t next_iter;
        
        err = fat32_get_fat_slot(dev, iter, &next_iter);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (FAT32_IS_EOC(next_iter)) {
            return FOS_EMPTY;
        }

        if (next_iter < 2 || next_iter >= dev->num_fat_slots) {
            return FOS_STATE_MISMATCH;
        }

        iter = next_iter;
    }
}

fernos_error_t fat32_get_dir_seq_sfn(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, uint32_t *sfn_entry_offset) {
    if (!sfn_entry_offset) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    // We'll start by advancing in the directory's cluster chain to save time when when use
    // the read and write functions above. No need to start at `slot_ind` if we skip the first few
    // clusters every time we read.
    uint32_t fwd_slot_ind;

    const uint32_t dir_entries_per_cluster = 
        (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    err = fat32_traverse_chain(dev, slot_ind, entry_offset / dir_entries_per_cluster, 
            &fwd_slot_ind);
    if (err == FOS_INVALID_INDEX) {
        return FOS_INVALID_INDEX;
    }

    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    const uint32_t fwd_entry_offset = entry_offset % dir_entries_per_cluster;
    uint32_t offset_iter = fwd_entry_offset;

    while (true) {
        // If we have already traversed the max number of entries, exit early!
        const uint32_t visited = offset_iter - fwd_entry_offset;
        if (visited >= FAT32_MAX_DIR_SEQ_LEN) {
            return FOS_STATE_MISMATCH;
        }

        fat32_dir_entry_t dir_entry;

        // We are somewhat safe to use this read function here because the max length of 
        // a used sequence is 21. Which is pretty small. There isn't really an advantage to
        // incrememnting some cluster index as we go.
        err = fat32_read_dir_entry(dev, fwd_slot_ind, offset_iter, &dir_entry);
        if (err == FOS_INVALID_INDEX) {
            return FOS_STATE_MISMATCH; // Here we were reading a valid sequence, but ended up
                                       // overshooting the end of the directory before the 
                                       // sequence ended!
        }

        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (dir_entry.raw[0] == FAT32_DIR_ENTRY_UNUSED || 
                dir_entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR) {
            // Here we reached the end of the sequence before reaching the SFN Entry :(.
            return FOS_STATE_MISMATCH;
        }

        if (!FT32F_ATTR_IS_LFN(dir_entry.short_fn.attrs)) {
            // We found our end! Wooh!
            // Remember, `visited` doesn't include this entry we are on right now.
            // Only ones we have previously processed completely.
            *sfn_entry_offset = entry_offset + visited;
            return FOS_SUCCESS;
        }

        // Otherwise, we just keep going!
        offset_iter++;
    }
}

fernos_error_t fat32_get_dir_seq_lfn(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sfn_entry_offset, uint16_t *lfn) {
    if (!lfn) {
        return FOS_BAD_ARGS;
    }

    const uint32_t dir_entries_per_cluster = 
        (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) / sizeof(fat32_dir_entry_t);

    uint32_t fwd_steps = sfn_entry_offset / dir_entries_per_cluster;
    uint32_t fwd_offset = sfn_entry_offset % dir_entries_per_cluster;

    // We step backwards one cluster at a time until the `fwd_offset` is >= MAX_SEQ_LEN - 1
    // (Or we run out of clusters).
    // 
    // The idea here is that when we traverse the cluster chain we can't overshoot the potential
    // beginning of the file's sequence.
    //
    // I am going to need to test this heavily because this is pretty confusing.
    while (fwd_offset < (FAT32_MAX_DIR_SEQ_LEN - 1) && fwd_steps > 0) {
        fwd_steps--;
        fwd_offset += dir_entries_per_cluster;
    }

    fernos_error_t err;

    uint32_t fwd_ind;
    err = fat32_traverse_chain(dev, slot_ind, fwd_steps, &fwd_ind);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    fat32_dir_entry_t entry;
    err = fat32_read_dir_entry(dev, fwd_ind, fwd_offset, &entry);
    if (err != FOS_SUCCESS) {
        return FOS_UNKNWON_ERROR;
    }

    // Confirm, that our first entry indexed is actually an SFN entry.
    if (entry.raw[0] == FAT32_DIR_ENTRY_UNUSED || entry.raw[0] == FAT32_DIR_ENTRY_TERMINTAOR ||
            FT32F_ATTR_IS_LFN(entry.short_fn.attrs)) {
        return FOS_STATE_MISMATCH; 
    }

    uint32_t chars_written = 0;

    while (true) {
        // Ok, first things first, let's back track.
        
        if (fwd_offset == 0) {
            goto end; // No where to go, that's ok.
        }

        fwd_offset--;

        err = fat32_read_dir_entry(dev, fwd_ind, fwd_offset, &entry);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (entry.raw[0] == FAT32_DIR_ENTRY_UNUSED ||
                !FT32F_ATTR_IS_LFN(entry.short_fn.attrs)) {
            goto end; // We hit an unused entry or the end of another chain.
        }

        // Aye yo, your sequence should never have more than MAX LFN LEN characters to write.
        if (chars_written >= FAT32_MAX_LFN_LEN) {
            return FOS_STATE_MISMATCH;
        }

        // Ok, here we should be gauranteed to be on an LFN Entry.
        //
        // Write out each piece to the LFN buffer, exit early if the NULL terminator is found.
        
        for (size_t i = 0; i < 5; i++) {
            if (entry.long_fn.long_fn_0[i] == 0) {
                goto end; 
            }
            lfn[chars_written++] = entry.long_fn.long_fn_0[i];
        }

        for (size_t i = 0; i < 6; i++) {
            if (entry.long_fn.long_fn_1[i] == 0) {
                goto end; 
            }
            lfn[chars_written++] = entry.long_fn.long_fn_1[i];
        }

        for (size_t i = 0; i < 2; i++) {
            if (entry.long_fn.long_fn_2[i] == 0) {
                goto end; 
            }
            lfn[chars_written++] = entry.long_fn.long_fn_2[i];
        }
    }

end:

    if (chars_written == 0) {
        // In the case no characters were written, no LFN entries were found, return EMPTY.
        return FOS_EMPTY;
    } 

    // Write out Null terminator and call it a day!
    lfn[chars_written] = 0; 

    return FOS_SUCCESS;
}

fernos_error_t fat32_check_sfn(fat32_device_t *dev, uint32_t slot_ind, const char *name,
        const char *ext) {
    if (!name || !ext) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    uint32_t seq_iter = 0;

    while (true) {
        uint32_t seq_start;

        err = fat32_next_dir_seq(dev, slot_ind, seq_iter, &seq_start);
        if (err == FOS_EMPTY) {
            return FOS_SUCCESS;
        }

        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        // Ok, now let's get the offset of this sequence's SFN entry.
        uint32_t sfn_entry_offset;

        err = fat32_get_dir_seq_sfn(dev, slot_ind, seq_start, &sfn_entry_offset);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        fat32_short_fn_dir_entry_t sfn_entry;
        err = fat32_read_dir_entry(dev, slot_ind, sfn_entry_offset, (fat32_dir_entry_t *)&sfn_entry);
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        if (mem_cmp(name, sfn_entry.short_fn, sizeof(sfn_entry.short_fn)) &&
                mem_cmp(ext, sfn_entry.extenstion, sizeof(sfn_entry.extenstion))) {
            return FOS_IN_USE;
        }

        seq_iter = sfn_entry_offset + 1;
    }
}

fernos_error_t fat32_check_lfn(fat32_device_t *dev, uint32_t slot_ind, const uint16_t *lfn) {
    if (!lfn) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    // Calc long filename length.
    uint32_t lfn_len = 0;
    while (lfn[lfn_len] != 0) {
        lfn_len++;
        if (lfn_len > FAT32_MAX_LFN_LEN) {
            return FOS_BAD_ARGS;
        }
    }

    if (lfn_len == 0) {
        return FOS_BAD_ARGS;
    }

    // Add one for the NT.
    uint16_t lfn_buf[FAT32_MAX_LFN_LEN + 1];

    uint32_t seq_iter = 0;

    while (true) {
        uint32_t seq_start;
        err = fat32_next_dir_seq(dev, slot_ind, seq_iter, &seq_start); 
        if (err == FOS_EMPTY) {
            return FOS_SUCCESS;
        }
         
        if (err != FOS_SUCCESS) {
            return FOS_UNKNWON_ERROR;
        }

        uint32_t sfn_entry_offset;
        err = fat32_get_dir_seq_sfn(dev, slot_ind, seq_start, &sfn_entry_offset);
        if (err != FOS_SUCCESS)  {
            return FOS_UNKNWON_ERROR;
        }

        err = fat32_get_dir_seq_lfn(dev, slot_ind, sfn_entry_offset, lfn_buf);

        // Remember, FOS_EMPTY means that this sequence doesn't even have a long filename.
        if (err != FOS_EMPTY) {
            if (err != FOS_SUCCESS) { // Other errors actually are problematic.
                return FOS_UNKNWON_ERROR;
            }

            if (mem_cmp(lfn_buf, lfn, lfn_len + 1)) {
                return FOS_IN_USE;
            }
        }

        // Advance past the sequence just processed.
        seq_iter = sfn_entry_offset + 1;
    }

    return FOS_SUCCESS;
}

fernos_error_t fat32_place_seq(fat32_device_t *dev, uint32_t slot_ind, uint32_t entry_offset,
        const fat32_short_fn_dir_entry_t *sfn_entry, const uint16_t *lfn) {
    if (!sfn_entry) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    const uint32_t dir_entries_per_cluster = (FAT32_REQ_SECTOR_SIZE * dev->sectors_per_cluster) /
        sizeof(fat32_dir_entry_t);

    uint32_t fwd_slot_ind;
    err = fat32_traverse_chain(dev, slot_ind, entry_offset / dir_entries_per_cluster, &fwd_slot_ind);
    if (err != FOS_SUCCESS) {
        return err;
    }

    const uint32_t fwd_entry_offset = entry_offset % dir_entries_per_cluster;

    // Calc number of LFN entries.

    uint32_t lfn_len = 0;
    if (lfn) {
        while (lfn[lfn_len] != 0) {
            lfn_len++;
            if (lfn_len > FAT32_MAX_LFN_LEN) {
                return FOS_BAD_ARGS;
            }
        }
    }
    const uint32_t num_lfn_entries = FAT32_NUM_LFN_ENTRIES(lfn_len);

    // First, write out the SFN entry.
    err = fat32_write_dir_entry(dev, fwd_slot_ind, fwd_entry_offset + num_lfn_entries, 
            (const fat32_dir_entry_t *)sfn_entry);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Write out LFN entries if there are any.
    if (num_lfn_entries > 0) {
        const uint8_t sfn_check_sum = fat32_checksum(sfn_entry->short_fn);

        fat32_long_fn_dir_entry_t lfn_entry;
        
        // We are going to do the lfn entry placement starting from the SFN entry, then walking
        // backwards.
        for (uint32_t lfn_iter = 0; lfn_iter < num_lfn_entries; lfn_iter++) {

            // 0 out the whole entry now so we don't need to deal with NULL terminators.
            mem_set(&lfn_entry, 0, sizeof(fat32_long_fn_dir_entry_t));

            lfn_entry.entry_order = lfn_iter + 1;
            if (lfn_iter == num_lfn_entries - 1) {
                lfn_entry.entry_order |= FAT32_LFN_START_PREFIX; // If this is the starting entry it must start with 0x40.
            }

            lfn_entry.attrs = FT32F_ATTR_LFN;
            lfn_entry.type = 0;
            lfn_entry.short_fn_checksum = sfn_check_sum;

            // determine what part of the LFN string we are copying.

            const uint32_t start = lfn_iter * FAT32_CHARS_PER_LFN_ENTRY;
            const uint32_t end = lfn_iter < num_lfn_entries - 1 ? (lfn_iter + 1) * FAT32_CHARS_PER_LFN_ENTRY : lfn_len;

            for (uint32_t char_i = start; char_i < end; char_i++) {
                const uint32_t rel = char_i % FAT32_CHARS_PER_LFN_ENTRY;

                // 0: 0-4
                // 1: 5-10
                // 2: 11-12
                if (rel <= 4) {
                    lfn_entry.long_fn_0[rel] = lfn[char_i];
                } else if (rel <= 10) {
                    lfn_entry.long_fn_1[rel - 5] = lfn[char_i];
                } else {
                    lfn_entry.long_fn_2[rel - 10] = lfn[char_i];
                }
            }

            // Ok, finally actually write out the LFN entry.
            err = fat32_write_dir_entry(dev, fwd_slot_ind, 
                    fwd_entry_offset + (num_lfn_entries - 1) - lfn_iter, 
                    (const fat32_dir_entry_t *)&lfn_entry);
            if (err != FOS_SUCCESS) {
                return err;
            }
        }
    }
    
    return FOS_SUCCESS;
}

void fat32_dump_dir(fat32_device_t *dev, uint32_t slot_ind, void (*pf)(const char *fmt, ...)) {
    if (!pf) {
        return;
    }

    // TODO: fill this bad boy in...
}
