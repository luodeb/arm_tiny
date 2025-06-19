#ifndef __VIRTIO_QUEUE_H__
#define __VIRTIO_QUEUE_H__

#include "virtio.h"
#include "tiny_types.h"

// 定义 size_t 类型
typedef unsigned long size_t;

// 队列内存布局计算宏
#define VIRTQ_DESC_SIZE(num) ((num) * sizeof(struct virtq_desc))
#define VIRTQ_AVAIL_SIZE(num) (sizeof(struct virtq_avail) + (num) * sizeof(uint16_t) + sizeof(uint16_t))
#define VIRTQ_USED_SIZE(num) (sizeof(struct virtq_used) + (num) * sizeof(struct virtq_used_elem) + sizeof(uint16_t))

// 队列内存对齐宏
#define VIRTQ_ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))

// 队列状态
#define VIRTQ_STATE_INACTIVE 0
#define VIRTQ_STATE_ACTIVE 1
#define VIRTQ_STATE_ERROR 2

// 描述符链操作
#define VIRTQ_DESC_CHAIN_END 0xFFFF

// 队列操作结果
#define VIRTQ_ADD_BUF_OK 0
#define VIRTQ_ADD_BUF_NO_SPACE -1
#define VIRTQ_ADD_BUF_INVALID -2

// 内存分配器接口
struct virtio_memory_allocator
{
    void *(*alloc)(size_t size, size_t align);
    void (*free)(void *ptr);
    uint64_t (*virt_to_phys)(void *virt);
    void *(*phys_to_virt)(uint64_t phys);
};

// 队列统计信息
struct virtqueue_stats
{
    uint64_t total_requests;      // 总请求数
    uint64_t completed_requests;  // 完成请求数
    uint64_t failed_requests;     // 失败请求数
    uint32_t max_queue_depth;     // 最大队列深度
    uint32_t current_queue_depth; // 当前队列深度
};

// 扩展的队列结构
struct virtqueue_extended
{
    struct virtqueue base; // 基础队列结构

    uint8_t state;       // 队列状态
    uint32_t total_size; // 总内存大小
    void *queue_memory;  // 队列内存起始地址

    struct virtqueue_stats stats;              // 统计信息
    struct virtio_memory_allocator *allocator; // 内存分配器

    // 同步原语（简化实现）
    volatile bool processing; // 处理中标志
};

// 队列管理 API
int virtio_queue_init_allocator(struct virtio_memory_allocator *allocator);
struct virtqueue *virtio_queue_create(struct virtio_device *vdev,
                                      uint16_t queue_index, uint16_t queue_size);
void virtio_queue_destroy(struct virtqueue *vq);
int virtio_queue_reset(struct virtqueue *vq);

// 简化的散列表结构（用于快速查找）
struct scatterlist
{
    uint64_t addr;   // 物理地址
    uint32_t length; // 长度
    uint16_t flags;  // 标志位
};

// 队列操作 API
int virtio_queue_add_sgl(struct virtqueue *vq,
                         struct scatterlist *sgl, uint16_t out_sgs, uint16_t in_sgs,
                         void *data);
void *virtio_queue_get_buf_timeout(struct virtqueue *vq, uint32_t *len, uint32_t timeout_ms);
bool virtio_queue_enable_cb(struct virtqueue *vq);
void virtio_queue_disable_cb(struct virtqueue *vq);
bool virtio_queue_is_broken(struct virtqueue *vq);

// 队列内存管理
size_t virtio_queue_calculate_memory_size(uint16_t queue_size);
int virtio_queue_setup_memory(struct virtqueue *vq, void *memory, size_t size);
void virtio_queue_cleanup_memory(struct virtqueue *vq);

// 队列状态查询
uint16_t virtio_queue_get_num_free(struct virtqueue *vq);
uint16_t virtio_queue_get_size(struct virtqueue *vq);
bool virtio_queue_is_full(struct virtqueue *vq);
bool virtio_queue_is_empty(struct virtqueue *vq);

// 队列调试和统计
void virtio_queue_dump_state(struct virtqueue *vq);
void virtio_queue_get_stats(struct virtqueue *vq, struct virtqueue_stats *stats);
void virtio_queue_reset_stats(struct virtqueue *vq);

// 内部辅助函数
static inline uint16_t virtio_queue_wrap_index(uint16_t index, uint16_t size)
{
    return index & (size - 1);
}

static inline bool virtio_queue_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old_idx)
{
    return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old_idx);
}

// 描述符链操作
uint16_t virtio_queue_alloc_desc_chain(struct virtqueue *vq, uint16_t count);
void virtio_queue_free_desc_chain(struct virtqueue *vq, uint16_t head);
int virtio_queue_setup_desc_chain(struct virtqueue *vq, uint16_t head,
                                  uint64_t *addrs, uint32_t *lens, uint16_t count,
                                  uint16_t write_count);

// 队列事件处理
typedef void (*virtqueue_callback_t)(struct virtqueue *vq);
int virtio_queue_set_callback(struct virtqueue *vq, virtqueue_callback_t callback);

#endif
