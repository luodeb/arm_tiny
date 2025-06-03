#include "tiny_types.h"
#include "tiny_io.h"
// #include <stdint.h>

#define PHYS_ADDR(x) ((uint32_t)((uintptr_t)(x) & 0xFFFFFFFF))

#define VIRTIO_MMIO_BASE 0x0A000000 // 假设你的virtio-mmio设备映射在这个地址
#define VIRTQ_SIZE 8

// Virtio MMIO寄存器偏移量
#define REG_MAGIC 0x00
#define REG_VERSION 0x04
#define REG_DEVICE_ID 0x08
#define REG_DRIVER_FEATURES 0x10
#define REG_STATUS 0x70

// Virtio设备状态
#define STATUS_ACKNOWLEDGE 1
#define STATUS_DRIVER 2
#define STATUS_DRIVER_OK 4

// 描述符 flags
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq
{
    struct virtq_desc desc[VIRTQ_SIZE];
    uint16_t avail_idx;
    // 其他队列字段简化处理...
};

static volatile uint32_t *reg = (uint32_t *)VIRTIO_MMIO_BASE;
static struct virtq *virtq;

void virtio_init()
{
    // 检查magic值
    if (reg[REG_MAGIC / 4] != 0x74726976)
    { // 'virt'
        tiny_printf(INFO, "Invalid magic\n");
        return;
    }

    // 重置设备
    reg[REG_STATUS / 4] = 0;
    reg[REG_STATUS / 4] = STATUS_ACKNOWLEDGE;
    reg[REG_STATUS / 4] |= STATUS_DRIVER;

    // 初始化virtqueue
    virtq = (struct virtq *)0x1000;  // 队列内存位置需要根据实际情况调整
    // reg[0x34 / 4] = (uint32_t)virtq; // QueuePFN寄存器设置物理地址
    reg[0x34/4] = PHYS_ADDR(virtq);

    // 设备激活
    reg[REG_STATUS / 4] |= STATUS_DRIVER_OK;
}

void test_read_block()
{
    // 创建描述符链
    uint8_t buffer[512] __attribute__((aligned(4)));
    virtq->desc[0].addr = (uint64_t)buffer;
    virtq->desc[0].len = sizeof(buffer);
    virtq->desc[0].flags = VIRTQ_DESC_F_WRITE;
    virtq->desc[0].next = 0;

    // 构造请求头（符合virtio-blk规范）
    struct virtio_blk_req
    {
        uint32_t type;
        uint32_t reserved;
        uint64_t sector;
    } req = {0, 0, 0};

    virtq->desc[1].addr = (uint64_t)&req;
    virtq->desc[1].len = sizeof(req);
    virtq->desc[1].flags = 0;
    virtq->desc[1].next = 0;

    // 提交请求
    virtq->avail_idx = 1; // 简化处理，实际需要操作avail ring

    // 通知设备
    reg[0x30 / 4] = 0; // QueueNotify

    // 等待完成（这里应该检查used ring，简化处理）
    for (volatile int i = 0; i < 1000000; i++)
        ;
}
