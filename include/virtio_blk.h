#ifndef __VIRTIO_BLK_H__
#define __VIRTIO_BLK_H__

#include "virtio.h"
#include "tiny_types.h"

// Virtio-blk 设备特性位
#define VIRTIO_BLK_F_SIZE_MAX 1    // 最大段大小
#define VIRTIO_BLK_F_SEG_MAX 2     // 最大段数量
#define VIRTIO_BLK_F_GEOMETRY 4    // 磁盘几何信息
#define VIRTIO_BLK_F_RO 5          // 只读设备
#define VIRTIO_BLK_F_BLK_SIZE 6    // 块大小
#define VIRTIO_BLK_F_FLUSH 9       // 缓存刷新支持
#define VIRTIO_BLK_F_TOPOLOGY 10   // 拓扑信息
#define VIRTIO_BLK_F_CONFIG_WCE 11 // 写缓存使能配置

// Virtio-blk 请求类型
#define VIRTIO_BLK_T_IN 0     // 读请求
#define VIRTIO_BLK_T_OUT 1    // 写请求
#define VIRTIO_BLK_T_FLUSH 4  // 刷新请求
#define VIRTIO_BLK_T_GET_ID 8 // 获取设备ID

// Virtio-blk 状态码
#define VIRTIO_BLK_S_OK 0     // 成功
#define VIRTIO_BLK_S_IOERR 1  // I/O错误
#define VIRTIO_BLK_S_UNSUPP 2 // 不支持的操作

// 块设备常量
#define VIRTIO_DEVICE_ID_BLOCK 2
#define VIRTIO_BLK_SECTOR_SIZE 512

// Virtio-blk 配置空间结构
struct virtio_blk_config
{
    uint64_t capacity; // 设备容量（扇区数）
    uint32_t size_max; // 最大段大小
    uint32_t seg_max;  // 最大段数量
    struct virtio_blk_geometry
    {
        uint16_t cylinders; // 柱面数
        uint8_t heads;      // 磁头数
        uint8_t sectors;    // 每磁道扇区数
    } geometry;
    uint32_t blk_size; // 块大小
    struct virtio_blk_topology
    {
        uint8_t physical_block_exp; // 物理块大小指数
        uint8_t alignment_offset;   // 对齐偏移
        uint16_t min_io_size;       // 最小I/O大小
        uint32_t opt_io_size;       // 最优I/O大小
    } topology;
    uint8_t writeback; // 写回缓存使能
    uint8_t unused0[3];
} __attribute__((packed));

// Virtio-blk 请求头
struct virtio_blk_req_header
{
    uint32_t type;     // 请求类型
    uint32_t reserved; // 保留字段
    uint64_t sector;   // 起始扇区号
} __attribute__((packed));

// 完整的 virtio-blk 请求结构
struct virtio_blk_request
{
    struct virtio_blk_req_header header; // 请求头
    uint8_t *data;                       // 数据缓冲区
    uint32_t data_len;                   // 数据长度
    uint8_t status;                      // 状态字节
    void *private_data;                  // 私有数据指针
};

// Virtio-blk 设备结构（简化版）
struct virtio_blk_device
{
    struct virtio_device *vdev;      // 基础 virtio 设备指针
    struct virtio_blk_config config; // 配置信息
    bool initialized;                // 初始化标志
};

// Virtio-blk API 函数
int virtio_blk_init(void);
struct virtio_blk_device *virtio_blk_get_device(void);
int virtio_blk_read_sectors(struct virtio_blk_device *blk_dev,
                            uint64_t sector, uint32_t count, void *buffer);
int virtio_blk_write_sectors(struct virtio_blk_device *blk_dev,
                             uint64_t sector, uint32_t count, const void *buffer);
int virtio_blk_flush(struct virtio_blk_device *blk_dev);
uint64_t virtio_blk_get_capacity(struct virtio_blk_device *blk_dev);
uint32_t virtio_blk_get_block_size(struct virtio_blk_device *blk_dev);

// 简化实现：不需要这些内部函数

// 测试函数
void virtio_blk_test(void);

// 简化的替代实现
int virtio_blk_simple_read_sector(struct virtio_blk_device *blk_dev,
                                  uint64_t sector, void *buffer);
void virtio_blk_simple_test(struct virtio_blk_device *blk_dev);

#endif
