
#pragma once

#include "s_block_device/fat32.h"

/*
 * Directory Functions
 *
 * A "dir entry" is a single 32-bit value within the directory.
 * A "dir sequence" is a continguous sequence of 32-bit "dir entry"'s for the information
 * of a single file.
 *
 * "entry offset" will usually denote the an index into a directory. (in units of "dir entries")
 */

/**
 * Create a new directory and write its starting slot index to `*slot_ind`.
 *
 * NOTE: This is really very simple under the hood. A "new directory" is really just a single
 * cluster who's first directory entry is set to 0.
 */
fernos_error_t fat32_new_dir(fat32_device_t *dev, uint32_t *slot_ind);

/**
 * Given a directory which starts at cluster `slot_ind`, read out directory entry
 * at `entry_offset`. (This is basically an index into array where each element has size 32-bytes)
 *
 * Read entry is copied into *entry.
 *
 * No checks if `entry_offset` comes before the terminator.
 * If `entry_offset` exceeds the directory sectors, FOS_INDEX_INVALID is returned.
 */
fernos_error_t fat32_read_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, fat32_dir_entry_t *entry);

/**
 * Given a directory which starts at cluster `slot_ind`, write to directory entry
 * at `entry_offset`. (This is basically an index into array where each element has size 32-bytes)
 *
 * No checks if `entry_offset` comes before the terminator.
 * If `entry_offset` exceeds the directory sectors, FOS_INDEX_INVALID is returned.
 */
fernos_error_t fat32_write_dir_entry(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, const fat32_dir_entry_t *entry);

/**
 * Find a sequence of free entires within a directory which has a length of at least `seq_len`.
 *
 * On Success, FOS_SUCCESS is returned, and the starting offset of the sequence is written to
 * `*entry_offset`.
 *
 * NOTE: if an initial search fails to find an adequate sequnce, the directory will be expanded.
 * If the expansion fails FOS_NO_SPACE will be returned.
 */
fernos_error_t fat32_get_free_seq(fat32_device_t *dev, uint32_t slot_ind, uint32_t seq_len,
        uint32_t *entry_offset);

/**
 * Given a sequence, set all of it's entries to unused.
 *
 * This function pretty much always returns FOS_SUCCESS unless there is some issue writing to
 * the device.
 *
 * This function will stop erasing when the given chain ends. If the chain is malformed.
 * For example, it has no SFN entry, this is OK. All the LFN entries will be erased up until
 * the terminator or an unused entry.
 */
fernos_error_t fat32_erase_seq(fat32_device_t *dev, uint32_t slot_ind, uint32_t entry_offset);

/**
 * Given the offset of an entry within a directory, get the offset of the start of 
 * the next directory sequence.
 *
 * If `entry_offset` points to the directory terminator, FOS_EMPTY is returned.
 * If `entry_offset` overshoots the directory sectors, FOS_EMPTY is returned.
 * If `entry_offset` points to an existing slot which comes AFTER the directory terminator,
 * undefined behavior.
 * If `entry_offset` already points to an LFN or SFN entry, `entry_offset` is written to 
 * `*seq_start`. FOS_SUCCES is returned.
 * Otherwise, the directory is traversed until an LFN/SFN entry is found. This offset is then
 * written to `*seq_start`. FOS_SUCCESS is returned.
 * If no SFN/LFN entry is found before hitting the terminator, FOS_EMPTY is returned.
 *
 * Other errors may be returned.
 */
fernos_error_t fat32_next_dir_seq(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, uint32_t *seq_start);

/**
 * Given the offset of a directory entry within a directory sequence, write the index 
 * of the sequence's SFN entry to `*sfn_entry_offset`.
 *
 * If entry offset points to an SFN entry, `entry_offset` is written.
 *
 * If an unused entry is encountered or the end of the directory is reached before finding
 * the SFN, return FOS_STATE_MISMATCH.
 *
 * Other errors may be returned.
 */
fernos_error_t fat32_get_dir_seq_sfn(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, uint32_t *sfn_entry_offset);

/**
 * Get a sequence's Long Filename given a pointer to its SFN entry.
 *
 * This function back tracks from the short file name entry over each of the LFN entries.
 * One by one filling the lfn buffer provided.
 *
 * `lfn` must have size at least FAT32_MAX_LFN_LEN + 1.
 *
 * If the given file has no LFN entries, FOS_EMPTY is returned.
 *
 * The contents written to `lfn` are only gauranteed to be valid when FOS_SUCCESS is returned.
 *
 * This will return an error if your sequence has too many LFN entries.
 */
fernos_error_t fat32_get_dir_seq_lfn(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sfn_entry_offset, uint16_t *lfn);

/**
 * This checks if a short filename is in use in a given directory.
 *
 * `name` should be 8 characters. (Space padded)
 * `ext` should be 3 characters. (Space padded)
 *
 * FOS_SUCCESS is returned if the name extension combo is yet to be used.
 * FOS_IN_USE is returned if the combination already exists in the given directory.
 */
fernos_error_t fat32_check_sfn(fat32_device_t *dev, uint32_t slot_ind, const char *name,
        const char *ext);

/**
 * This checks if a long filename is in use in a directory.
 *
 * `lfn` is a NULL terminated string. Shouldn't have length than FAT32_MAK_LFN_LEN.
 *
 * FOS_SUCCESS if the name is yet to be used.
 * FOS_IN_USE if the name is already in use.
 */
fernos_error_t fat32_check_lfn(fat32_device_t *dev, uint32_t slot_ind, const uint16_t *lfn);

/**
 * This call alloactes a new directory sequence containing the given lfn and sfn.
 *
 * If the allocation fails, FOS_NO_SPACE is returned.
 * NOTE: This call DOES NOT check for name uniqueness. You should do that yourself before calling
 * this function.
 *
 * If you don't want your sequence to have LFN entries, specify `lfn` as NULL.
 * If the given long file name is too long, an error will be returned.
 *
 * Returns FOS_SUCCESS if there is no error writing to the directory.
 */
fernos_error_t fat32_new_seq(fat32_device_t *dev, uint32_t slot_ind,
        const fat32_short_fn_dir_entry_t *sfn_entry, const uint16_t *lfn, uint32_t *entry_offset);

/**
 * Somewhat up to the implementor what this does.
 * Just use `pf` to out put some representation of the directory starting at `slot_ind`.
 */
void fat32_dump_dir(fat32_device_t *dev, uint32_t slot_ind, void (*pf)(const char *fmt, ...));
