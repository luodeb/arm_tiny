#ifndef __VIRTIO_MMIO_H__
#define __VIRTIO_MMIO_H__

#include "tiny_types.h"

// VirtIO MMIO register offsets
#define VIRTIO_MMIO_MAGIC 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c
#define VIRTIO_MMIO_QUEUE_PFN 0x040
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

// VirtIO magic number
#define VIRTIO_MAGIC_VALUE 0x74726976

// VirtIO device IDs
#define VIRTIO_DEVICE_ID_NET 1
#define VIRTIO_DEVICE_ID_BLOCK 2

// VirtIO 2.0 feature bits (common features)
#define VIRTIO_F_VERSION_1 32
#define VIRTIO_F_ACCESS_PLATFORM 33
#define VIRTIO_F_RING_PACKED 34
#define VIRTIO_F_IN_ORDER 35
#define VIRTIO_F_ORDER_PLATFORM 36
#define VIRTIO_F_SR_IOV 37
#define VIRTIO_F_NOTIFICATION_DATA 38

// Feature negotiation masks
#define VIRTIO_COMMON_FEATURES_MASK ((1ULL << VIRTIO_F_VERSION_1))
#define VIRTIO_SUPPORTED_FEATURES_MASK (VIRTIO_COMMON_FEATURES_MASK)

// VirtIO MMIO device scanning
#define VIRTIO_MMIO_BASE_ADDR 0x0a000000
#define VIRTIO_MMIO_DEVICE_SIZE 0x200
#define VIRTIO_MMIO_MAX_DEVICES 32

// VirtIO device status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED 128

// VirtIO queue descriptor flags
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_DESC_F_INDIRECT 4

// VirtIO queue available flags
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

// VirtIO queue used flags
#define VIRTQ_USED_F_NO_NOTIFY 1

// VirtIO device structure
typedef struct
{
    uint64_t base_addr;
    uint32_t magic;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t queue_num_max;
    bool ready;
} virtio_device_t;

// VirtIO queue descriptor
typedef struct
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;

// VirtIO queue available ring
typedef struct
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtq_avail_t;

// VirtIO queue used element
typedef struct
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_used_elem_t;

// VirtIO queue used ring
typedef struct
{
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];
} __attribute__((packed)) virtq_used_t;

// VirtIO queue structure
typedef struct
{
    uint32_t queue_size;
    uint64_t desc_table_addr;
    uint64_t avail_ring_addr;
    uint64_t used_ring_addr;
    uint16_t last_used_idx;
    virtq_desc_t *desc;
    virtq_avail_t *avail;
    virtq_used_t *used;
} virtqueue_t;

// Function declarations
bool virtio_probe_device(uint64_t base_addr);
bool virtio_device_init(virtio_device_t *dev, uint64_t base_addr);
bool virtio_queue_init(virtio_device_t *dev, uint32_t queue_idx);
void virtio_set_status(virtio_device_t *dev, uint8_t status);
uint32_t virtio_read32(uint64_t addr);
void virtio_write32(uint64_t addr, uint32_t value);
virtio_device_t *virtio_get_device(void);
virtqueue_t *virtio_get_queue(void);
bool virtio_queue_add_descriptor(uint16_t desc_idx, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next);
bool virtio_queue_submit_request(uint16_t desc_head, uint32_t queue_idx);
bool virtio_queue_wait_for_completion(void);
uint64_t virtio_scan_devices(uint32_t target_device_id);

// Cache management functions for DMA coherency
void virtio_cache_clean_range(uint64_t start, uint32_t size);
void virtio_cache_invalidate_range(uint64_t start, uint32_t size);

#endif // __VIRTIO_MMIO_H__
