/*
 * simple_fs.c - 简单文件系统实现
 *
 * 实现基本的 FAT32 文件系统支持，能够读取根目录和文件内容
 */

#include "simple_fs.h"
#include "virtio_blk.h"
#include "config.h"
#include "tiny_io.h"
#include "tiny_types.h"

// 全局文件系统上下文
static struct filesystem g_fs;
static bool g_fs_initialized = false;

// 内部函数声明
static int fs_detect_type(void);

// 简单的字符串比较函数
static int simple_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// 简单的内存复制函数
static void simple_memcpy(void *dest, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
}

// 初始化文件系统
int fs_init(struct virtio_blk_device *blk_dev)
{
    if (g_fs_initialized)
    {
        return FS_OK;
    }

    if (!blk_dev)
    {
        return FS_ERROR_INVALID;
    }

    tiny_printf(INFO, "fs: initializing file system\n");

    // 初始化文件系统上下文
    g_fs.blk_dev = blk_dev;
    g_fs.mounted = false;
    g_fs.buffer_size = FS_SECTOR_SIZE * 8; // 8个扇区的缓冲区

    // 分配扇区缓冲区
    g_fs.sector_buffer = (uint8_t *)FS_BUFFER_BASE_ADDR;

    // 检测文件系统类型
    int ret = fs_detect_type();
    if (ret != FS_OK)
    {
        tiny_printf(WARN, "fs: failed to detect file system type\n");
        return ret;
    }

    g_fs_initialized = true;
    tiny_printf(INFO, "fs: file system initialized, type=%s\n", fs_get_type_string(g_fs.sb.fs_type));

    return FS_OK;
}

// 清理文件系统
void fs_cleanup(void)
{
    if (g_fs.mounted)
    {
        fs_unmount();
    }
    g_fs_initialized = false;
}

// 挂载文件系统
int fs_mount(void)
{
    if (!g_fs_initialized)
    {
        return FS_ERROR_INVALID;
    }

    if (g_fs.mounted)
    {
        return FS_OK;
    }

    tiny_printf(INFO, "fs: mounting file system\n");

    // 根据文件系统类型进行挂载
    switch (g_fs.sb.fs_type)
    {
    case FS_TYPE_FAT32:
        // FAT32 已在检测时解析完成
        break;
    default:
        return FS_ERROR_NOT_SUPPORTED;
    }

    g_fs.mounted = true;
    tiny_printf(INFO, "fs: file system mounted successfully\n");

    return FS_OK;
}

// 卸载文件系统
void fs_unmount(void)
{
    g_fs.mounted = false;
    tiny_printf(INFO, "fs: file system unmounted\n");
}

// 检测文件系统类型
static int fs_detect_type(void)
{
    // 读取第一个扇区（引导扇区）
    int ret = virtio_blk_read_sectors(g_fs.blk_dev, 0, 1, g_fs.sector_buffer);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "fs: failed to read boot sector\n");
        return FS_ERROR_IO;
    }

    // 尝试解析为 FAT32
    struct fat32_boot_sector *boot = (struct fat32_boot_sector *)g_fs.sector_buffer;

    // 检查 FAT32 签名
    tiny_printf(DEBUG, "fs: boot signature: 0x%x\n", boot->boot_signature);
    if (boot->boot_signature == 0x29 &&
        (boot->fs_type[0] == 'F' && boot->fs_type[1] == 'A' && boot->fs_type[2] == 'T'))
    {

        ret = fat32_parse_boot_sector(boot);
        if (ret == FS_OK)
        {
            g_fs.sb.fs_type = FS_TYPE_FAT32;
            return FS_OK;
        }
    }

    // 检查引导扇区签名
    uint16_t *signature = (uint16_t *)(g_fs.sector_buffer + 510);
    tiny_printf(DEBUG, "fs: boot signature: 0x%x\n", *signature);
    if (*signature == 0xAA55)
    {
        // 可能是其他文件系统，但我们只支持 FAT32
        tiny_printf(INFO, "fs: found boot signature but not FAT32\n");
    }

    g_fs.sb.fs_type = FS_TYPE_UNKNOWN;
    return FS_ERROR_NOT_SUPPORTED;
}

// 解析 FAT32 引导扇区
int fat32_parse_boot_sector(struct fat32_boot_sector *boot)
{
    // 基本检查
    if (boot->bytes_per_sector != 512)
    {
        tiny_printf(WARN, "fs: unsupported sector size %d\n", boot->bytes_per_sector);
        return FS_ERROR_NOT_SUPPORTED;
    }

    // 填充超级块信息
    g_fs.sb.sector_size = boot->bytes_per_sector;
    g_fs.sb.cluster_size = boot->sectors_per_cluster;
    g_fs.sb.fat_sector = boot->reserved_sectors;
    g_fs.sb.fat_size = boot->fat_size_32;
    g_fs.sb.total_sectors = boot->total_sectors_32;

    // 计算根目录起始扇区
    uint32_t data_start = boot->reserved_sectors + (boot->num_fats * boot->fat_size_32);
    uint32_t root_cluster = boot->root_cluster;
    g_fs.sb.root_dir_sector = data_start + (root_cluster - 2) * boot->sectors_per_cluster;
    g_fs.sb.root_dir_size = boot->sectors_per_cluster;

    tiny_printf(INFO, "fs: FAT32 detected - sectors=%d, cluster_size=%d, root_sector=%d\n",
                g_fs.sb.total_sectors, g_fs.sb.cluster_size, (uint32_t)g_fs.sb.root_dir_sector);

    return FS_OK;
}

// 列出目录内容
int fs_list_directory(const char *path, struct dir_entry **entries, uint32_t *count)
{
    if (!g_fs.mounted || !entries || !count)
    {
        return FS_ERROR_INVALID;
    }

    *entries = NULL;
    *count = 0;

    // 目前只支持根目录
    if (path && path[0] != '/' && path[1] != '\0')
    {
        return FS_ERROR_NOT_FOUND;
    }

    switch (g_fs.sb.fs_type)
    {
    case FS_TYPE_FAT32:
        return fat32_read_directory(g_fs.sb.root_dir_sector, entries, count);
    default:
        return FS_ERROR_NOT_SUPPORTED;
    }
}

// 释放目录项列表
void fs_free_dir_entries(struct dir_entry *entries)
{
    // 简单实现：不实际释放内存
    (void)entries;
}

// 读取 FAT32 目录
int fat32_read_directory(uint64_t dir_sector, struct dir_entry **entries, uint32_t *count)
{
    // 读取目录扇区
    int ret = virtio_blk_read_sectors(g_fs.blk_dev, dir_sector, g_fs.sb.root_dir_size, g_fs.sector_buffer);
    if (ret != VIRTIO_OK)
    {
        return FS_ERROR_IO;
    }

    // 分配目录项数组（静态分配）
    static struct dir_entry dir_entries[32];
    uint32_t entry_count = 0;

    struct fat32_dir_entry *fat_entries = (struct fat32_dir_entry *)g_fs.sector_buffer;
    uint32_t max_entries = (g_fs.sb.root_dir_size * FS_SECTOR_SIZE) / sizeof(struct fat32_dir_entry);

    for (uint32_t i = 0; i < max_entries && entry_count < 32; i++)
    {
        struct fat32_dir_entry *fat_entry = &fat_entries[i];

        // 检查目录项是否有效
        if (fat_entry->name[0] == 0x00)
        {
            break; // 目录结束
        }

        if (fat_entry->name[0] == 0xE5)
        {
            continue; // 已删除的项
        }

        if (fat_entry->attr & 0x08)
        {
            continue; // 卷标
        }

        if (fat_entry->attr & 0x0F)
        {
            continue; // 长文件名项
        }

        // 解析目录项
        fat32_parse_dir_entry(fat_entry, &dir_entries[entry_count].info);
        dir_entries[entry_count].next = (entry_count < 31) ? &dir_entries[entry_count + 1] : NULL;
        entry_count++;
    }

    if (entry_count > 0)
    {
        dir_entries[entry_count - 1].next = NULL;
    }

    *entries = (entry_count > 0) ? dir_entries : NULL;
    *count = entry_count;

    tiny_printf(INFO, "fs: found %d entries in directory\n", entry_count);
    return FS_OK;
}

// 解析 FAT32 目录项
void fat32_parse_dir_entry(struct fat32_dir_entry *fat_entry, struct file_info *info)
{
    // 转换文件名
    fat32_convert_filename((const char *)fat_entry->name, info->name);

    // 设置文件大小和类型
    info->size = fat_entry->file_size;
    info->type = (fat_entry->attr & 0x10) ? FILE_TYPE_DIRECTORY : FILE_TYPE_REGULAR;

    // 计算起始簇号
    uint32_t cluster = ((uint32_t)fat_entry->first_cluster_high << 16) | fat_entry->first_cluster_low;

    // 转换为扇区号
    if (cluster >= 2)
    {
        uint32_t data_start = g_fs.sb.fat_sector + (2 * g_fs.sb.fat_size); // 假设2个FAT表
        info->start_sector = data_start + (cluster - 2) * g_fs.sb.cluster_size;
        info->sector_count = (info->size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
    }
    else
    {
        info->start_sector = 0;
        info->sector_count = 0;
    }

    info->attributes = fat_entry->attr;
}

// 转换 FAT32 文件名格式
void fat32_convert_filename(const char *fat_name, char *output)
{
    int out_pos = 0;

    // 复制文件名部分
    for (int i = 0; i < 8 && fat_name[i] != ' '; i++)
    {
        output[out_pos++] = fat_name[i];
    }

    // 添加扩展名
    if (fat_name[8] != ' ')
    {
        output[out_pos++] = '.';
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++)
        {
            output[out_pos++] = fat_name[i];
        }
    }

    output[out_pos] = '\0';
}

// 获取文件系统类型字符串
const char *fs_get_type_string(uint32_t fs_type)
{
    switch (fs_type)
    {
    case FS_TYPE_FAT32:
        return "FAT32";
    case FS_TYPE_EXT2:
        return "EXT2";
    default:
        return "Unknown";
    }
}

// 获取文件类型字符串
const char *fs_get_file_type_string(uint32_t file_type)
{
    switch (file_type)
    {
    case FILE_TYPE_REGULAR:
        return "File";
    case FILE_TYPE_DIRECTORY:
        return "Directory";
    case FILE_TYPE_SYMLINK:
        return "Symlink";
    default:
        return "Unknown";
    }
}

// 基本测试函数
void fs_test_basic_operations(void)
{
    tiny_printf(INFO, "fs: running basic file system test\n");

    if (!g_fs.mounted)
    {
        tiny_printf(WARN, "fs: file system not mounted\n");
        return;
    }

    // 列出根目录
    struct dir_entry *entries;
    uint32_t count;

    int ret = fs_list_directory("/", &entries, &count);
    if (ret == FS_OK)
    {
        tiny_printf(INFO, "fs: root directory contains %d entries:\n", count);

        struct dir_entry *current = entries;
        while (current)
        {
            struct file_info *info = &current->info;
            tiny_printf(INFO, "  %s (%s, %d bytes)\n",
                        info->name, fs_get_file_type_string(info->type), info->size);
            current = current->next;
        }
    }
    else
    {
        tiny_printf(WARN, "fs: failed to list root directory, error %d\n", ret);
    }
}

// 查找指定文件名的文件
struct file_info *fs_find_file_by_name(const char *filename)
{
    if (!g_fs.mounted || !filename)
    {
        return NULL;
    }

    // 获取根目录内容
    struct dir_entry *entries;
    uint32_t count;

    int ret = fs_list_directory("/", &entries, &count);
    if (ret != FS_OK)
    {
        return NULL;
    }

    // 查找指定文件
    struct dir_entry *current = entries;
    while (current)
    {
        if (simple_strcmp(current->info.name, filename) == 0)
        {
            // 返回静态分配的文件信息副本
            static struct file_info found_file;
            found_file = current->info;
            return &found_file;
        }
        current = current->next;
    }

    return NULL;
}

// 读取文件内容
int fs_read_file_content(const char *filename, void *buffer, uint32_t buffer_size, uint32_t *bytes_read)
{
    if (!g_fs.mounted || !filename || !buffer || !bytes_read)
    {
        return FS_ERROR_INVALID;
    }

    *bytes_read = 0;

    // 查找文件
    struct file_info *file = fs_find_file_by_name(filename);
    if (!file)
    {
        tiny_printf(WARN, "fs: file '%s' not found\n", filename);
        return FS_ERROR_NOT_FOUND;
    }

    // 检查文件类型
    if (file->type != FILE_TYPE_REGULAR)
    {
        tiny_printf(WARN, "fs: '%s' is not a regular file\n", filename);
        return FS_ERROR_INVALID;
    }

    // 检查文件大小
    if (file->size > FS_MAX_FILE_READ_SIZE)
    {
        tiny_printf(WARN, "fs: file '%s' too large (%d bytes, max %d)\n",
                    filename, file->size, FS_MAX_FILE_READ_SIZE);
        return FS_ERROR_INVALID;
    }

    // 检查缓冲区大小
    if (buffer_size < file->size)
    {
        tiny_printf(WARN, "fs: buffer too small for file '%s' (need %d, have %d)\n",
                    filename, file->size, buffer_size);
        *bytes_read = file->size; // 返回实际需要的大小
        return FS_ERROR_INVALID;
    }

    // 计算需要读取的扇区数
    uint32_t sectors_needed = (file->size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
    if (sectors_needed == 0)
    {
        sectors_needed = 1; // 至少读取一个扇区
    }

    // 读取文件数据
    int ret = virtio_blk_read_sectors(g_fs.blk_dev, file->start_sector, sectors_needed, g_fs.sector_buffer);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "fs: failed to read sectors for file '%s'\n", filename);
        return FS_ERROR_IO;
    }

    // 复制文件内容到用户缓冲区
    simple_memcpy(buffer, g_fs.sector_buffer, file->size);
    *bytes_read = file->size;

    tiny_printf(DEBUG, "fs: successfully read %d bytes from file '%s'\n", file->size, filename);
    return FS_OK;
}

// 测试文件读取功能
void fs_test_file_reading(void)
{
    tiny_printf(INFO, "fs: testing file reading functionality\n");

    if (!g_fs.mounted)
    {
        tiny_printf(WARN, "fs: file system not mounted, skipping file read test\n");
        return;
    }

    // 分配读取缓冲区
    static uint8_t read_buffer[1024];
    uint32_t bytes_read;

    // 测试读取 hello.txt
    int ret = fs_read_file_content("hello.txt", read_buffer, sizeof(read_buffer), &bytes_read);
    if (ret == FS_OK)
    {
        tiny_printf(INFO, "fs: successfully read hello.txt (%d bytes):\n", bytes_read);
        tiny_printf(INFO, "  Content: ");
        for (uint32_t i = 0; i < bytes_read && i < 100; i++)
        {
            if (read_buffer[i] >= 32 && read_buffer[i] <= 126)
            {
                tiny_printf(NONE, "%c", read_buffer[i]);
            }
            else if (read_buffer[i] == '\n')
            {
                tiny_printf(NONE, "\\n");
            }
            else if (read_buffer[i] == '\r')
            {
                tiny_printf(NONE, "\\r");
            }
            else
            {
                tiny_printf(NONE, "\\x%02x", read_buffer[i]);
            }
        }
        tiny_printf(NONE, "\n");
    }
    else
    {
        tiny_printf(WARN, "fs: failed to read hello.txt, error %d\n", ret);
    }

    // 测试读取 readme.txt
    ret = fs_read_file_content("readme.txt", read_buffer, sizeof(read_buffer), &bytes_read);
    if (ret == FS_OK)
    {
        tiny_printf(INFO, "fs: successfully read readme.txt (%d bytes):\n", bytes_read);
        tiny_printf(INFO, "  Content: ");
        for (uint32_t i = 0; i < bytes_read && i < 100; i++)
        {
            if (read_buffer[i] >= 32 && read_buffer[i] <= 126)
            {
                tiny_printf(NONE, "%c", read_buffer[i]);
            }
            else if (read_buffer[i] == '\n')
            {
                tiny_printf(NONE, "\\n");
            }
            else if (read_buffer[i] == '\r')
            {
                tiny_printf(NONE, "\\r");
            }
            else
            {
                tiny_printf(NONE, "\\x%02x", read_buffer[i]);
            }
        }
        tiny_printf(NONE, "\n");
    }
    else
    {
        tiny_printf(WARN, "fs: failed to read readme.txt, error %d\n", ret);
    }

    tiny_printf(INFO, "fs: file reading test completed\n");
}
