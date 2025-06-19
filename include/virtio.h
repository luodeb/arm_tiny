#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "tiny_types.h"

// Virtio MMIO 寄存器偏移量
#define VIRTIO_MMIO_MAGIC 0x000
#define VIRTIO_MMIO_MAGIC_VALUE 0x74726976
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_READY 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

// Virtio 设备状态位
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 64
#define VIRTIO_STATUS_FAILED 128

// Virtio 设备类型
#define VIRTIO_DEVICE_TYPE_NETWORK 1
#define VIRTIO_DEVICE_TYPE_BLOCK 2
#define VIRTIO_DEVICE_TYPE_CONSOLE 3

// Virtio 队列描述符标志
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_DESC_F_INDIRECT 4

// Virtio 中断标志
#define VIRTIO_MMIO_INT_VRING (1 << 0)
#define VIRTIO_MMIO_INT_CONFIG (1 << 1)

// 队列大小限制
#define VIRTIO_QUEUE_MAX_SIZE 1024
#define VIRTIO_QUEUE_DEFAULT_SIZE 128

// 内存对齐要求
#define VIRTIO_QUEUE_ALIGN 4096

// VirtIO MMIO 地址空间配置
#define VIRTIO_MMIO_BASE 0x0a003e00
#define VIRTIO_MMIO_SIZE 0x200
#define VIRTIO_MMIO_STRIDE 0x200
#define VIRTIO_MAX_DEVICES 8

// 错误码
#define VIRTIO_OK 0
#define VIRTIO_ERROR_INVALID_DEVICE -1
#define VIRTIO_ERROR_NO_MEMORY -2
#define VIRTIO_ERROR_NO_DEVICE -3
#define VIRTIO_ERROR_QUEUE_FULL -4
#define VIRTIO_ERROR_TIMEOUT -5
#define VIRTIO_ERROR_IO -6
#define VIRTIO_ERROR_NOT_SUPPORTED -7

// 定义 NULL
#ifndef NULL
#define NULL ((void *)0)
#endif

// 前向声明
struct virtio_device;
struct virtqueue;
struct virtq_desc;
struct virtq_avail;
struct virtq_used;

// 函数类型定义
typedef int (*virtio_device_init_func)(struct virtio_device *vdev);

// Virtio 队列描述符
struct virtq_desc
{
    uint64_t addr;  // 缓冲区物理地址
    uint32_t len;   // 缓冲区长度
    uint16_t flags; // 标志位
    uint16_t next;  // 下一个描述符索引
};

// Virtio 可用环
struct virtq_avail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[]; // 可变长度数组
};

// Virtio 已用环元素
struct virtq_used_elem
{
    uint32_t id;  // 描述符链头索引
    uint32_t len; // 写入的字节数
};

// Virtio 已用环
struct virtq_used
{
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[]; // 可变长度数组
};

// Virtio 队列结构
struct virtqueue
{
    uint16_t queue_index;   // 队列索引
    uint16_t queue_size;    // 队列大小
    uint16_t last_used_idx; // 上次处理的 used 索引
    uint16_t free_head;     // 空闲描述符链头
    uint16_t num_free;      // 空闲描述符数量

    struct virtq_desc *desc;   // 描述符表
    struct virtq_avail *avail; // 可用环
    struct virtq_used *used;   // 已用环

    void **data;                // 描述符对应的数据指针
    struct virtio_device *vdev; // 所属设备
    void *queue_memory;         // 队列内存起始地址
};

// Virtio 设备结构
struct virtio_device
{
    uint32_t base_addr;       // MMIO 基地址
    uint32_t device_id;       // 设备ID
    uint32_t vendor_id;       // 厂商ID
    uint32_t device_features; // 设备特性
    uint32_t driver_features; // 驱动特性
    uint8_t status;           // 设备状态

    struct virtqueue *queues[16]; // 队列数组
    uint16_t num_queues;          // 队列数量

    // 设备特定的操作函数指针
    int (*init)(struct virtio_device *vdev);
    void (*cleanup)(struct virtio_device *vdev);
    int (*handle_interrupt)(struct virtio_device *vdev);
};

// 核心 API 函数
int virtio_init(void);
struct virtio_device *virtio_find_device(uint32_t device_id);
int virtio_init_device(struct virtio_device *vdev, virtio_device_init_func init_func);

// 简化实现：不使用队列

// 寄存器访问函数
uint32_t virtio_read_reg32(struct virtio_device *vdev, uint32_t offset);
void virtio_write_reg32(struct virtio_device *vdev, uint32_t offset, uint32_t value);

// 测试函数
void virtio_test(void);

#endif
