
#pragma once

#include <stdint.h>

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


typedef struct _fat32_fs_info_sector_t fat32_fs_info_sector_t;
struct _fat32_fs_info_sector_t {
    /**
     * Must be: 0x52 0x52 0x61 0x41
     * Signifies FAT32.
     */
    uint8_t signature0[4];

    /**
     * Should be set to 0 at init time.
     */
    uint8_t zeros[480];

    /**
     * Must be: 0x72 0x72 0x41 0x61
     */
    uint8_t signature1[4];

    



} __attribute__ ((packed));
