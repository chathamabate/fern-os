
#pragma once

#include <stdint.h>
#include "s_util/misc.h"

typedef struct _fat_bios_param_block_2_0_t {
    /**
     * Probably should only ever be 512 or 4K.
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

    /**
     * SHOULD BE 0 FOR FAT32
     */
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
     * Otherwise, the 3 boot sectors should be copied starting at this sector.
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

// Next the information section I guess???
// I think you would be correct about this!

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
