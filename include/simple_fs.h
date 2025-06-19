#ifndef __SIMPLE_FS_H__
#define __SIMPLE_FS_H__

#include "tiny_types.h"
#include "virtio_blk.h"

// 文件系统类型
#define FS_TYPE_UNKNOWN 0
#define FS_TYPE_FAT32 1
#define FS_TYPE_EXT2 2

// 文件类型
#define FILE_TYPE_REGULAR 1
#define FILE_TYPE_DIRECTORY 2
#define FILE_TYPE_SYMLINK 3

// 错误码
#define FS_OK 0
#define FS_ERROR_NOT_FOUND -1
#define FS_ERROR_NO_MEMORY -2
#define FS_ERROR_IO -3
#define FS_ERROR_INVALID -4
#define FS_ERROR_NOT_SUPPORTED -5

// 最大路径长度
#define FS_MAX_PATH_LEN 256
#define FS_MAX_NAME_LEN 64

// 文件内容读取常量
#define FS_MAX_FILE_READ_SIZE (64 * 1024) // 最大读取64KB文件

// 文件信息结构
struct file_info
{
    char name[FS_MAX_NAME_LEN]; // 文件名
    uint32_t size;              // 文件大小
    uint32_t type;              // 文件类型
    uint64_t start_sector;      // 起始扇区
    uint32_t sector_count;      // 扇区数量
    uint32_t attributes;        // 文件属性
};

// 目录项结构
struct dir_entry
{
    struct file_info info;  // 文件信息
    struct dir_entry *next; // 下一个目录项
};

// 文件系统超级块（简化版）
struct fs_superblock
{
    uint32_t fs_type;         // 文件系统类型
    uint64_t total_sectors;   // 总扇区数
    uint32_t sector_size;     // 扇区大小
    uint64_t root_dir_sector; // 根目录起始扇区
    uint32_t root_dir_size;   // 根目录大小
    uint32_t cluster_size;    // 簇大小（FAT32）
    uint64_t fat_sector;      // FAT表起始扇区（FAT32）
    uint32_t fat_size;        // FAT表大小（FAT32）
};

// 文件系统上下文
struct filesystem
{
    struct virtio_blk_device *blk_dev; // 块设备
    struct fs_superblock sb;           // 超级块
    bool mounted;                      // 挂载状态
    uint8_t *sector_buffer;            // 扇区缓冲区
    uint32_t buffer_size;              // 缓冲区大小
};

// FAT32 特定结构
struct fat32_boot_sector
{
    uint8_t jump[3];             // 跳转指令
    char oem_name[8];            // OEM名称
    uint16_t bytes_per_sector;   // 每扇区字节数
    uint8_t sectors_per_cluster; // 每簇扇区数
    uint16_t reserved_sectors;   // 保留扇区数
    uint8_t num_fats;            // FAT表数量
    uint16_t root_entries;       // 根目录项数（FAT32为0）
    uint16_t total_sectors_16;   // 总扇区数（16位）
    uint8_t media_type;          // 媒体类型
    uint16_t fat_size_16;        // FAT大小（16位，FAT32为0）
    uint16_t sectors_per_track;  // 每磁道扇区数
    uint16_t num_heads;          // 磁头数
    uint32_t hidden_sectors;     // 隐藏扇区数
    uint32_t total_sectors_32;   // 总扇区数（32位）
    uint32_t fat_size_32;        // FAT大小（32位）
    uint16_t ext_flags;          // 扩展标志
    uint16_t fs_version;         // 文件系统版本
    uint32_t root_cluster;       // 根目录簇号
    uint16_t fs_info;            // FS信息扇区
    uint16_t backup_boot_sector; // 备份引导扇区
    uint8_t reserved[12];        // 保留
    uint8_t drive_number;        // 驱动器号
    uint8_t reserved1;           // 保留
    uint8_t boot_signature;      // 引导签名
    uint32_t volume_id;          // 卷ID
    char volume_label[11];       // 卷标
    char fs_type[8];             // 文件系统类型
} __attribute__((packed));

struct fat32_dir_entry
{
    char name[11];               // 文件名（8.3格式）
    uint8_t attr;                // 属性
    uint8_t nt_reserved;         // NT保留
    uint8_t create_time_tenth;   // 创建时间（十分之一秒）
    uint16_t create_time;        // 创建时间
    uint16_t create_date;        // 创建日期
    uint16_t last_access_date;   // 最后访问日期
    uint16_t first_cluster_high; // 起始簇号（高16位）
    uint16_t write_time;         // 写入时间
    uint16_t write_date;         // 写入日期
    uint16_t first_cluster_low;  // 起始簇号（低16位）
    uint32_t file_size;          // 文件大小
} __attribute__((packed));

// 文件系统 API
int fs_init(struct virtio_blk_device *blk_dev);
void fs_cleanup(void);
int fs_mount(void);
void fs_unmount(void);

// 目录操作
int fs_list_directory(const char *path, struct dir_entry **entries, uint32_t *count);
void fs_free_dir_entries(struct dir_entry *entries);
int fs_change_directory(const char *path);
char *fs_get_current_directory(void);

// 文件操作
int fs_open_file(const char *filename, struct file_info *info);
int fs_read_file(const struct file_info *info, uint32_t offset,
                 void *buffer, uint32_t size, uint32_t *bytes_read);
int fs_file_exists(const char *filename);
int fs_get_file_info(const char *filename, struct file_info *info);

// 新增文件内容读取功能
int fs_read_file_content(const char *filename, void *buffer, uint32_t buffer_size, uint32_t *bytes_read);
struct file_info *fs_find_file_by_name(const char *filename);
void fs_test_file_reading(void);

// 工具函数
const char *fs_get_type_string(uint32_t fs_type);
const char *fs_get_file_type_string(uint32_t file_type);
uint32_t fs_get_total_space(void);
uint32_t fs_get_free_space(void);

// 内部函数（FAT32特定）
int fat32_parse_boot_sector(struct fat32_boot_sector *boot);
int fat32_read_directory(uint64_t dir_sector, struct dir_entry **entries, uint32_t *count);
void fat32_parse_dir_entry(struct fat32_dir_entry *fat_entry, struct file_info *info);
void fat32_convert_filename(const char *fat_name, char *output);

// 调试和测试函数
void fs_dump_superblock(void);
void fs_test_basic_operations(void);
int fs_create_test_file(const char *filename, const char *content);

#endif
