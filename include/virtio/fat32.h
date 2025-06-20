#ifndef __FAT32_H__
#define __FAT32_H__

#include "tiny_types.h"

// FAT32 Boot Sector structure
typedef struct
{
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed)) fat32_boot_sector_t;

// FAT32 Directory Entry structure
typedef struct
{
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

// FAT32 file attributes
#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME 0x0F

// FAT32 cluster values
#define FAT32_EOC 0x0FFFFFF8
#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define FAT32_FREE_CLUSTER 0x00000000

// FAT32 file system structure
typedef struct
{
    fat32_boot_sector_t boot_sector;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t root_dir_cluster;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    bool initialized;
} fat32_fs_t;

// Function declarations
bool fat32_init(void);
bool fat32_read_file(const char *filename, char *buffer, uint32_t max_size);
bool fat32_parse_boot_sector(void);
uint32_t fat32_get_next_cluster(uint32_t cluster);
bool fat32_read_cluster(uint32_t cluster, void *buffer);
bool fat32_find_file_in_dir(uint32_t dir_cluster, const char *filename, fat32_dir_entry_t *entry);

#endif // __FAT32_H__
