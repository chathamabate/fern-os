
#pragma once

#include <stdint.h>
#include "s_util/misc.h"
#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"
#include "s_data/list.h"
#include "s_util/rand.h"

#define FAT32_MASK          (0x0FFFFFFFU)
#define FAT32_EOC           (0x0FFFFFF8U)
#define FAT32_IS_EOC(v)     ((v & FAT32_EOC) == FAT32_EOC)
#define FAT32_BAD_CLUSTER   (0x0FFFFFF7U)

#define FAT32_SLOTS_PER_FAT_SECTOR (512 / sizeof(uint32_t))

/**
 * Maximum length of a file's long file name.
 * Remeber, this is in 16-bit characters. (Does NOT include 16-bit NULL terminator 0x0000)
 */
#define FAT32_MAX_FN_LEN (255U)

typedef struct _fat_bios_param_block_2_0_t {
    /**
     * Probably should only ever be 512 or 4K.
     * We'll require 512 for now.
     */
    uint16_t bytes_per_sector;
    
    /**
     * Must be power of 2 in sequence 1, 2, 4, ..., 128
     */
    uint8_t sectors_per_cluster;

    /**
     * Number of sectors before first FAT.
     *
     * Should be at least 1 for the VBR.
     */
    uint16_t reserved_sectors;

    /**
     * Probably 2.
     */
    uint8_t num_fats;

    /**
     * SHOULD BE 0 FAT FAT32
     */
    uint16_t num_small_fat_root_dir_entires;

    uint16_t num_sectors;

    /**
     * See some FAT ID table for what value to use.
     *
     * For a partition, we should use 0xF8.
     */
    uint8_t media_descriptor;

    /**
     * SHOULD BE 0 FOR FAT32
     */
    uint16_t sectors_per_fat;
} __attribute__ ((packed)) fat_bios_param_block_2_0_t;

typedef struct _fat_bios_param_block_3_31_t {
    fat_bios_param_block_2_0_t super;

    /**
     * Since we will be using LBA, I think this should be set to 1.
     */
    uint16_t sectors_per_track;

    /**
     * Since we will be using LBA, I think this should be set to 1.
     */
    uint16_t heads_per_disk;

    /**
     * On non partitioned system, this must be 0.
     * On a partitioned system, this should be the number of sectors which
     * come before this volume.
     */
    uint32_t volume_absolute_offset;

    /**
     * total logical sectors. Should only be used if super.num_sectors is 0.
     */
    uint32_t num_sectors;
} __attribute__ ((packed)) fat_bios_param_block_3_31_t;

typedef struct _fat32_extended_bios_param_block_t {
    fat_bios_param_block_3_31_t super;

    uint32_t sectors_per_fat;

    /**
     * See wikipedia on how to use these.
     * Should be all 0. Meaning FAT mirroring is enabled.
     */
    uint16_t ext_flags;

    uint8_t minor_version;
    uint8_t major_version;

    /**
     * Cluster number of root directory start. (Usually cluster 2)
     */
    uint32_t root_dir_cluster;

    /**
     * Offset of FS Info sector. (Usually 1, i.e. 2nd sector of the boot sectors)
     */
    uint16_t fs_info_sector;

    /**
     * If 0 or 0xFFFF, there is no backup.
     *
     * Otherwise, the boot sector should be copied starting at this sector.
     */
    uint16_t boot_sectors_copy_sector_start;

    /**
     * Should be initialized to 0.
     */
    uint8_t reserved0[12];

    /**
     * Probably should be 0x0 when using just 1 drive.
     */
    uint8_t drive_number;

    uint8_t reserved1;

    /**
     * Should be 0x29.
     */
    uint8_t extended_boot_signature;

    uint32_t serial_number;

    /**
     * Space padded. NOT null terminated.
     * For display purposes only.
     */
    char volume_label[11];

    /**
     * Space padded. NOT null terminated.
     * For display purposes only.
     */
    char fs_label[8];
} __attribute__ ((packed)) fat32_extended_bios_param_block_t;

typedef struct _fat32_fs_boot_sector_t {
    uint8_t jmp_instruction[3];

    /**
     * Not NULL terminated, padded with spaces.
     */
    char oem_name[8];

    /**
     * extended param block.
     */
    fat32_extended_bios_param_block_t fat32_ebpb;

    /**
     * Boot code.
     */
    uint8_t boot_code[512 - (13 + sizeof(fat32_extended_bios_param_block_t))];

    /**
     * Must be 0x55 0xAA.
     */
    uint8_t boot_signature[2];
} __attribute__ ((packed)) fat32_fs_boot_sector_t;

typedef struct _fat32_fs_info_sector_t {
    /**
     * Must be: 0x52 0x52 0x61 0x41
     * Signifies FAT32.
     */
    uint8_t signature0[4];

    /**
     * Should be set to 0 at init time.
     */
    uint8_t reserved0[480];

    /**
     * Must be: 0x72 0x72 0x41 0x61
     */
    uint8_t signature1[4];

    /**
     * Last known number of free clusters. 0xFFFFFFFF if unknown.
     */
    uint32_t num_free_clusters;    

    /**
     * Most recently allocated data cluster.
     */
    uint32_t last_allocated_date_cluster;

    /**
     * Should be set to 0 at init time.
     */
    uint8_t reserved1[12];

    /**
     * Must be: 0x00 0x00 0x55 0xAA
     */
    uint8_t signature2[4];
} __attribute__ ((packed)) fat32_fs_info_sector_t;

/*
 * FAT32 Date/time helpers.
 */

/**
 * [0:4] Day of month (1 - 31)
 * [5:8] Month of Year (1 - 12)
 * [9:15] Count of Year from 1980 (0 - 127)
 */
typedef uint16_t fat32_date_t;

#define FT32D_DAY_OFF (0)
#define FT32D_DAY_WID (5)
#define FT32D_DAY_WID_MASK TO_MASK(FT32D_DAY_WID)
#define FT32D_DAY_MASK (FT32D_DAY_WID_MASK << FT32D_DAY_OFF)

#define FT32D_MONTH_OFF (5)
#define FT32D_MONTH_WID (4)
#define FT32D_MONTH_WID_MASK TO_MASK(FT32D_MONTH_WID)
#define FT32D_MONTH_MASK (FT32D_MONTH_WID_MASK << FT32D_MONTH_OFF)

#define FT32D_YEAR_OFF (9)
#define FT32D_YEAR_WID (7)
#define FT32D_YEAR_WID_MASK TO_MASK(FT32D_YEAR_WID)
#define FT32D_YEAR_MASK (FT32D_YEAR_WID_MASK << FT32D_YEAR_OFF)

static inline uint8_t fat32_date_get_day(fat32_date_t d) {
    return (uint8_t)((d & FT32D_DAY_MASK) >> FT32D_DAY_OFF);
}

static inline void fat32_date_set_day(fat32_date_t *d, uint8_t day) {
    fat32_date_t date = *d;

    date &= ~FT32D_DAY_MASK;
    date |= (day << FT32D_DAY_OFF) & FT32D_DAY_MASK;

    *d = date;
}

static inline uint8_t fat32_date_get_month(fat32_date_t d) {
    return (uint8_t)((d & FT32D_MONTH_MASK) >> FT32D_MONTH_OFF);
}

static inline void fat32_date_set_month(fat32_date_t *d, uint8_t month) {
    fat32_date_t date = *d;

    date &= ~FT32D_MONTH_MASK;
    date |= (month << FT32D_MONTH_OFF) & FT32D_MONTH_MASK;

    *d = date;
}

static inline uint8_t fat32_date_get_year(fat32_date_t d) {
    return (uint8_t)((d & FT32D_YEAR_MASK) >> FT32D_YEAR_OFF);
}

static inline void fat32_date_set_year(fat32_date_t *d, uint8_t year) {
    fat32_date_t date = *d;

    date &= ~FT32D_YEAR_MASK;
    date |= (year << FT32D_YEAR_OFF) & FT32D_YEAR_MASK;

    *d = date;
}

static inline fat32_date_t fat32_date(uint8_t month, uint8_t day, uint8_t year) {
    fat32_date_t date; 

    fat32_date_set_month(&date, month);
    fat32_date_set_day(&date, day);
    fat32_date_set_year(&date, year);

    return date;
}

/**
 * [0:4] Seconds / 2 (0-29)
 * [5:10] Minutes  (0-59)
 * [11:15] Hours (0-23)
 */
typedef uint16_t fat32_time_t;

#define FT32T_SECS_OFF (0)
#define FT32T_SECS_WID (5)
#define FT32T_SECS_WID_MASK TO_MASK(FT32T_SECS_WID)
#define FT32T_SECS_MASK (FT32T_SECS_WID_MASK << FT32T_SECS_OFF)

#define FT32T_MINS_OFF (5)
#define FT32T_MINS_WID (6)
#define FT32T_MINS_WID_MASK TO_MASK(FT32T_MINS_WID)
#define FT32T_MINS_MASK (FT32T_MINS_WID_MASK << FT32T_MINS_OFF)

#define FT32T_HOURS_OFF (11)
#define FT32T_HOURS_WID (5)
#define FT32T_HOURS_WID_MASK TO_MASK(FT32T_HOURS_WID)
#define FT32T_HOURS_MASK (FT32T_HOURS_WID_MASK << FT32T_HOURS_OFF)

static inline uint8_t fat32_time_get_seconds(fat32_time_t t) {
    return (uint8_t)((t & FT32T_SECS_MASK) >> FT32T_SECS_OFF);
}

static inline void fat32_time_set_secs(fat32_time_t *t, uint8_t secs) {
    fat32_time_t time = *t;

    time &= ~FT32T_SECS_MASK;
    time |= (secs << FT32T_SECS_OFF) & FT32T_SECS_MASK;

    *t = time;
}

static inline uint8_t fat32_time_get_mins(fat32_time_t t) {
    return (uint8_t)((t & FT32T_MINS_MASK) >> FT32T_MINS_OFF);
}

static inline void fat32_time_set_mins(fat32_time_t *t, uint8_t mins) {
    fat32_time_t time = *t;

    time &= ~FT32T_MINS_MASK;
    time |= (mins << FT32T_MINS_OFF) & FT32T_MINS_MASK;

    *t = time;
}

static inline uint8_t fat32_time_get_hours(fat32_time_t t) {
    return (uint8_t)((t & FT32T_HOURS_MASK) >> FT32T_HOURS_OFF);
}

static inline void fat32_time_set_hours(fat32_time_t *t, uint8_t hours) {
    fat32_time_t time = *t;

    time &= ~FT32T_HOURS_MASK;
    time |= (hours << FT32T_HOURS_OFF) & FT32T_HOURS_MASK;

    *t = time;
}

static inline fat32_time_t fat32_time(uint8_t hours, uint8_t mins, uint8_t secs) {
    fat32_time_t time;

    fat32_time_set_hours(&time, hours);
    fat32_time_set_mins(&time, mins);
    fat32_time_set_secs(&time, secs);

    return time;
}

/*
 * Now for Directory entries.
 */

#define FT32F_ATTR_RO           (1UL << 0)
#define FT32F_ATTR_HIDDEN       (1UL << 1)
#define FT32F_ATTR_SYSTEM       (1UL << 2)
#define FT32F_ATTR_VOL_LABEL    (1UL << 3)
#define FT32F_ATTR_SUBDIR       (1UL << 4)
#define FT32F_ATTR_ARCHIVE      (1UL << 5)
#define FT32F_ATTR_LFN          (0x0FU)

#define FT32F_ATTR_IS_LFN(attr) (((attr) & FT32F_ATTR_LFN) == FT32F_ATTR_LFN)

typedef struct _fat32_short_fn_dir_entry_t {
    /**
     * Space padded.
     * All CAPS.
     */
    char short_fn[8];

    /**
     * Space padded.
     * All CAPS.
     */
    char extenstion[3];

    uint8_t attrs;

    uint8_t reserved;

    /**
     * All 4 of the creation time fields are optional.
     */
    uint8_t creation_time_hundredths;
    fat32_time_t creation_time;
    fat32_date_t creation_date;
    fat32_date_t last_access_date;

    uint16_t first_cluster_high;
    
    /**
     * Required.
     */
    fat32_time_t last_write_time;
    fat32_date_t last_write_date;

    uint16_t first_cluster_low;
    
    /**
     * 0 for directories.
     * This is in a unit of bytes.
     */
    uint32_t files_size;
} __attribute__ ((packed)) fat32_short_fn_dir_entry_t;

typedef struct _fat32_long_fn_dir_entry_t {
    /**
     * 1 Based number of this long file name entry.
     * I believe this counts down.
     *
     * The first entry to be read is really the last part of the long name.
     * This is OR'd with 0x40 if this the "last" (actually the first) long file name entry.
     *
     * 7th bit must ALWAYS be 0 I think.
     */
    uint8_t entry_order;

    /**
     * 5 UTF-16 characters. (I may just support ascii anyway tbh)
     *
     * For all filename segments, after the NULL terminator comes 0xFFFF pads.
     */
    uint16_t long_fn_0[5];

    /**
     * Must be 0x0F
     */
    uint8_t attrs;

    /**
     * Must be 0.
     */
    uint8_t type;

    uint8_t short_fn_checksum;

    uint16_t long_fn_1[6];

    /**
     * Should always be set to 0.
     */
    uint8_t reserved[2];

    uint16_t long_fn_2[2];
} __attribute__ ((packed)) fat32_long_fn_dir_entry_t;

/**
 * If the first byte of a directory entry holds this value,
 * it is unallocated and free to use!
 */
#define FAT32_DIR_ENTRY_UNUSED (0xE5U)

/**
 * If the first byte of a directory entry holds this value,
 * you have reached the end of the directory. This entry and all following entries
 * are unusable! (Unless you move advance the terminator that is)
 */
#define FAT32_DIR_ENTRY_TERMINTAOR (0x0U)

typedef union _fat32_dir_entry_t {
    /**
     * Use raw[0] to determine if this entry is used or not!
     */

    // Raw bytes.
    uint8_t raw[sizeof(fat32_short_fn_dir_entry_t)];

    fat32_short_fn_dir_entry_t short_fn;
    fat32_long_fn_dir_entry_t long_fn;
} fat32_dir_entry_t;

/**
 * This is going to be an intermediary interface between a block device and a file system.
 * 
 * The FAT32 device will provide helpers for using the FAT32 specific structures!
 */
typedef struct _fat32_device_t {
    /**
     * NOTE: All "offset" fields below are in sectors.
     * *Unless stated otherwise*
     *
     * All "cluster" fields are indeces into the FAT.
     */

    allocator_t * const al;

    const bool delete_wrapped_bd;

    block_device_t * const bd;

    /**
     * Absolute offset of the partition start within the BD.
     *
     * All offsets below are relative to this number!
     */
    const uint32_t bd_offset;

    /**
     * Number of sectors in this partition. NOT size of the block device.
     */
    const uint32_t num_sectors;

    /**
     * All "offset" fields below are relative to `bd_offset` above.
     *
     * Some of these fields will be directly from the boot sector. It's nice not
     * to need to read disk everytime we want to know simple information about the FS.
     */

    /**
     * Sector where first FAT begins.
     */
    const uint16_t fat_offset;

    /**
     * FAT redundancy
     */
    const uint8_t num_fats;

    /**
     * Remember that the space the FAT addresses may be smaller or larger than how much
     * space is actually available. 
     */
    const uint32_t sectors_per_fat;

    /**
     * Where the data clusters begin. 
     * Remember, there are no data clusters 0 or 1.
     */
    const uint32_t data_section_offset;

    const uint8_t sectors_per_cluster;

    /**
     * Number of slots in the FAT.
     *
     * Remember, this means there will be num_fat_slots - 2 clusters after the FAT section
     * as Clusters 0 and 1 don't exist!
     */
    const uint32_t num_fat_slots;

    /**
     * The cluster of the root directory.
     */
    const uint32_t root_dir_cluster;

    /**
     * Used for behaviors of the fat32 device which require randomness.
     */
    rand_t r;

    /**
     * This array holds indeces of FAT slots whose corresponding clusters are not in use.
     */
    uint32_t free_q[FAT32_SLOTS_PER_FAT_SECTOR];

    /**
     * free_q[0]...free_q[free_q_fill - 1] are occupied in the free_q array.
     */
    uint32_t free_q_fill;
} fat32_device_t;

/**
 * Compute the standard FAT32 checksum for a short filename.
 */
uint8_t fat32_checksum(const char *short_fn);

/**
 * Given these parameters, calculate how many sectors should be in each FAT.
 */
uint32_t compute_sectors_per_fat(uint32_t total_sectors, uint16_t bytes_per_sector, 
        uint16_t reserved_sectors, uint8_t fat_copies,  uint8_t sectors_per_cluster);

/**
 * Initialize a Block device with the FAT32 structure.
 *
 * offset is where in the block device you'd like the partition to begin. NOTE: this does NOT
 * set any sort of global partition table if offset > 0. This is in unit of sectors.
 *
 * num_sectors is the number of total sectors which will be occupied by the partition.
 *
 * An error is returned if the given block device doesn't have 512 bit sectors.
 *
 * sectors_per_cluster must be a power of 2 in the range [1, 128]
 *
 * Remeber, this function just initializes a FAT32 file system. Later functions for manipulating
 * the file system must be able to work properly on any FAT32 instance, not just the one generated 
 * by this instance.
 */
fernos_error_t init_fat32(block_device_t *bd, uint32_t offset, uint32_t num_sectors, 
        uint32_t sectors_per_cluster);

/**
 * Parse/Validate a FAT32 partition. 
 * `offset` should be the very beginning of the parition in `bd`. (unit of sectors)
 *
 * If `dubd` is True (delete underlying block device), the given block device will be deleted
 * when this fat32 device is deleted. Otherwise, the bd will persist after this device is 
 * deleted.
 *
 * On success, a new fat32_device_t will be written to *dev_out.
 */
fernos_error_t parse_new_fat32_device(allocator_t *al, block_device_t *bd, uint32_t offset, 
        uint64_t seed, bool dubd, fat32_device_t **dev_out);

static inline fernos_error_t parse_new_da_fat32_device(block_device_t *bd, uint32_t offset, 
        uint64_t seed, bool dubd, fat32_device_t **dev_out) {
    return parse_new_fat32_device(get_default_allocator(), bd, offset, seed, dubd, dev_out);
}

void delete_fat32_device(fat32_device_t *dev);

/**
 * Flush underlying block device. You will likely want to sync the FATs before doing this.
 */
static inline fernos_error_t fat32_flush(fat32_device_t *dev) {
    return bd_flush(dev->bd);
}

/**
 * Get the size of a cluster in bytes!
 */
static inline size_t fat32_cluster_size(fat32_device_t *dev) {
    return bd_sector_size(dev->bd) * dev->sectors_per_cluster;
}

/**
 * Get a data cluster's entry in FAT 0.
 */
fernos_error_t fat32_get_fat_slot(fat32_device_t *dev, uint32_t slot_ind, uint32_t *out_val);

/**
 * Set a data slot_ind's entry in FAT 0.
 */
fernos_error_t fat32_set_fat_slot(fat32_device_t *dev, uint32_t slot_ind, uint32_t val);

/**
 * Copy the contents of FAT 0 into all the other FATs.
 */
fernos_error_t fat32_sync_fats(fat32_device_t *dev);

/**
 * Traverse a chain starting at `slot_ind`. Each slot in the chain will be written over
 * with value 0.
 *
 * If the chain is malformed, FOS_STATE_MISMATCH is returned.
 * If there is an error with the block device, FOS_UNKNOWN_ERROR is returned.
 */
fernos_error_t fat32_free_chain(fat32_device_t *dev, uint32_t slot_ind);

/**
 * Pop a free slot from the free queue and write it to *slot_ind.
 * (The free slot is given the temporary value of EOC before being returned)
 *
 * If a free slot cannot be found FOS_NO_SPACE is returned.
 * If there is some other error, FOS_UNKNOWN_ERROR is returned.
 * Otherwise FOS_SUCCESS is returned.
 */
fernos_error_t fat32_pop_free_fat_slot(fat32_device_t *dev, uint32_t *slot_ind);

/**
 * Attempt to create a new chain of length `len`.
 *
 * If we run out of disk space before claiming `len` clusters, the entire chain created thus far
 * is freed. FOS_NO_SPACE is returned.
 *
 * If there is some unexpected error, a partial/malformed chain may be left over in the BD.
 * In this case, FOS_UNKNOWN_ERROR is returned.
 *
 * FOS_SUCCESS is ONLY returned when the ENTIRE chain is successfully allocated. When this 
 * happens, the starting slot index is written to *slot_ind.
 */
fernos_error_t fat32_new_chain(fat32_device_t *dev, uint32_t len, uint32_t *slot_ind);

/**
 * Attempt to resize the given chain to length `new_len`.
 *
 * If `new_len` < the length of the chain, clusters will be freed.
 * If `new_len` > the length of the chain, clusters will be allocated.
 *
 * When extending a chain, if space runs out, all clusters which were newly allocated will be freed.
 * The given chain will remain at its original length. FOS_NO_SPACE will be returned.
 * If there is some funky error with the underlying block device, FOS_UNKNOWN_ERROR is returned.
 * If there is a malformed chain, FOS_STATE_MISMATCH is returned.
 *
 * FOS_SUCCESS is returned if and only if the given chain now has exact length `new_len`.
 * (A `new_len` value of 0, will free the entire chain)
 */
fernos_error_t fat32_resize_chain(fat32_device_t *dev, uint32_t slot_ind, uint32_t new_len);

/**
 * Use this function if you want the slot index of a cluster within a chain.
 *
 * `slot_offset` >= the length of the chain, FOS_INVALID_INDEX is returned.
 * (Errors can also be returned if there are issues with the block device)
 *
 * Otherwise, FOS_SUCCESS is returned, and the requested slot index is written to *slot_stop_ind.
 */
fernos_error_t fat32_traverse_chain(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t slot_offset, uint32_t *slot_stop_ind);

/**
 * Read sectors of a chain into `dest`.
 *
 * `dest` must have size at least `num_sectors` * sector size.
 *
 * If the chain is not large enough, FOS_INVALID_RANGE will be returned.
 */
fernos_error_t fat32_read(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t sector_offset, uint32_t num_sectors, void *dest);

/**
 * Write sectors of a chain from `src`.
 *
 * `src` must have size at least `num_sectors` * sector size.
 *
 * If the chain is not large enough, FOS_INVALID_RANGE will be returned.
 */
fernos_error_t fat32_write(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t num_sectors, const void *src);

/*
 * The read/write piece functions below are implemented to use the bd read/write piece functions.
 * The idea once again is that they MIGHT be much more efficient for read/writing small pieces
 * of data.
 */

/**
 * Read bytes from just ONE sector within a chain.
 * 
 * `dest` must have size at least `len`.
 *
 * If `sector_offset` is too large, FOS_INVALID_INDEX is returned.
 */
fernos_error_t fat32_read_piece(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t byte_offset, uint32_t len, void *dest);

/**
 * Write bytes to just ONE sector within a chain.
 *
 * `len` must be < sector size.
 * `src` must have size at least `len`.
 *
 * If `sector_offset` is too large, FOS_INVALID_INDEX is returned.
 */
fernos_error_t fat32_write_piece(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t sector_offset, uint32_t byte_offset, uint32_t len, const void *src);

/*
 * Directory Functions
 *
 * A "dir entry" is a single 32-bit value within the directory.
 * A "dir sequence" is a continguous sequence of 32-bit "dir entry"'s for the information
 * of a single file.
 *
 * "entry offset" will usually denote the an index into a directory. (in units of "dir entries")
 *
 * A "valid entry" is an entry within the directory which is known to be before the terminator.
 * (Or if there is no terminatory, just exists on in the directory sectors)
 */

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
 * Does the given directory have any valid directory entires?
 * (i.e. is it not empty)
 *
 * Returns FOS_SUCCESS if so, FOS_EMPTY if empty.
 * Other errors may be returned.
 *
 * If FOS_SUCCESS is returned, it is gauranteed that entry with offset 0 is valid.
 */
fernos_error_t fat32_dir_has_valid_entries(fat32_device_t *dev, uint32_t slot_ind);

/**
 * Given a VALID entry offset within a directory, determine if the entry at `entry_offset + 1`
 * is also VALID.
 *
 * If the next entry is Valid, FOS_SUCCESS is returned.
 * If the next entry is the terminator, FOS_EMPTY is returned.
 * If the next entry doesn't exist on disk, FOS_EMPTY is returned.
 * Other error may be returned.
 *
 * NOTE: This function is intended to be used during iteration. Undefined behavior if the
 * given entry is invalid.
 */
fernos_error_t fat32_dir_entry_has_next(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset);

/**
 *
 */
fernos_error_t fat32_next_dir_seq(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, uint32_t *seq_start);

/**
 * Given the offset of a directory entry within a directory sequence, write the index 
 * of the sequence's SFN entry to `*sfn_entry_offset`.
 *
 * If entry offset points to an SFN entry, `entry_offset` is written.
 *
 * If an unused entry is encounter FOS_STATE_MISMATCH will be returned. 
 */
fernos_error_t fat32_traverse_dir_seq(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, uint32_t *sfn_entry_offset);


/**
 *
 */
fernos_error_t fat32_next_dir_seq(fat32_device_t *dev, uint32_t slot_ind,
        uint32_t entry_offset, uint32_t *seq_entry_offset);


fernos_error_t fat32_next_dir_entry(fat32_device_t *dev, uint32_t slot_ind

/**
 * Read out file information for a file whose entries start at index `entry_offset` within
 * a directory which begins at cluster `slot_ind`.
 *
 * NOTE: For the sake of performance, this function DOES NOT search the full given directory for 
 * its NULL Terminator to determine if `entry_offset` is valid. If the directory's cluster chain
 * is long enough to be indexed by `entry_offset`, this file assumes `entry_offset` is in bounds
 * for the directory itself.
 *
 * Only interpret *info when FOS_SUCCESS is returned.
 */
fernos_error_t fat32_read_file_info(fat32_device_t *dev, uint32_t slot_ind, 
        uint32_t entry_offset, fat32_file_info_t *info);
