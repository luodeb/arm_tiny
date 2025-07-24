#ifndef _VIRTIO_MMIO_H
#define _VIRTIO_MMIO_H

#include "tiny_types.h"
#include "virtio_allocator.h"

// VirtIO MMIO register offsets
#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028 // Legacy only
#define VIRTIO_MMIO_QUEUE_SEL 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c // Legacy only
#define VIRTIO_MMIO_QUEUE_PFN 0x040   // Legacy only
#define VIRTIO_MMIO_QUEUE_READY 0x044 // Modern only
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_STATUS 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080    // Modern only
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084   // Modern only
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090   // Modern only
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094  // Modern only
#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0    // Modern only
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4   // Modern only
#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc // Modern only
#define VIRTIO_MMIO_CONFIG 0x100

// VirtIO magic value
#define VIRTIO_MMIO_MAGIC 0x74726976

// VirtIO device IDs
#define VIRTIO_ID_NET 1
#define VIRTIO_ID_BLOCK 2
#define VIRTIO_ID_CONSOLE 3
#define VIRTIO_ID_RNG 4
#define VIRTIO_ID_BALLOON 5
#define VIRTIO_ID_SCSI 8
#define VIRTIO_ID_9P 9
#define VIRTIO_ID_GPU 16

// VirtIO status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 64
#define VIRTIO_STATUS_FAILED 128

// VirtIO queue descriptor flags
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_DESC_F_INDIRECT 4

// VirtIO available ring flags
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

// VirtIO used ring flags
#define VIRTQ_USED_F_NO_NOTIFY 1

// VirtIO queue descriptor
typedef struct
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;

// VirtIO available ring
typedef struct
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtq_avail_t;

// VirtIO used ring element
typedef struct
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_used_elem_t;

// VirtIO used ring
typedef struct
{
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];
} __attribute__((packed)) virtq_used_t;

// VirtIO queue structure
typedef struct
{
    uint32_t queue_id;
    uint32_t queue_size;
    uint64_t desc_addr;
    uint64_t avail_addr;
    uint64_t used_addr;
    virtq_desc_t *desc;
    virtq_avail_t *avail;
    virtq_used_t *used;
    uint16_t last_used_idx;
    uint16_t free_head;
    uint16_t num_free;
} virtio_queue_t;

// VirtIO device structure
typedef struct
{
    uint64_t base_addr;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint64_t device_features;
    uint64_t driver_features;
    uint8_t status;
    uint32_t device_index;     // Device index for memory allocation
    virtio_queue_t queues[16]; // Support up to 16 queues
    uint32_t num_queues;
} virtio_device_t;

// Function declarations
int virtio_mmio_init(virtio_device_t *dev, uint64_t base_addr, uint32_t device_index);
int virtio_queue_setup(virtio_device_t *dev, uint32_t queue_id, uint32_t queue_size);
int virtio_queue_add_buf(virtio_device_t *dev, uint32_t queue_id,
                         uint64_t *buffers, uint32_t *lengths,
                         uint32_t out_num, uint32_t in_num);
int virtio_queue_kick(virtio_device_t *dev, uint32_t queue_id);
int virtio_queue_get_buf(virtio_device_t *dev, uint32_t queue_id, uint32_t *len);
void virtio_set_status(virtio_device_t *dev, uint8_t status);
uint8_t virtio_get_status(virtio_device_t *dev);
uint8_t virtio_read8(virtio_device_t *dev, uint32_t offset);
void virtio_write8(virtio_device_t *dev, uint32_t offset, uint8_t value);
uint16_t virtio_read16(virtio_device_t *dev, uint32_t offset);
void virtio_write16(virtio_device_t *dev, uint32_t offset, uint16_t value);
uint32_t virtio_read32(virtio_device_t *dev, uint32_t offset);
void virtio_write32(virtio_device_t *dev, uint32_t offset, uint32_t value);

#endif // _VIRTIO_MMIO_H
