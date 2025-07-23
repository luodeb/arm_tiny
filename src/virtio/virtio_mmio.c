#include "virtio/virtio_mmio.h"
#include "tinystd.h"
#include "tinyio.h"

// Memory barrier functions
static inline void mb(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void rmb(void)
{
    __asm__ volatile("dsb ld" ::: "memory");
}

static inline void wmb(void)
{
    __asm__ volatile("dsb st" ::: "memory");
}

uint32_t virtio_read32(virtio_device_t *dev, uint32_t offset)
{
    uint32_t value = read32((volatile void *)(dev->base_addr + offset));
    rmb();
    return value;
}

void virtio_write32(virtio_device_t *dev, uint32_t offset, uint32_t value)
{
    wmb();
    write32(value, (volatile void *)(dev->base_addr + offset));
}

void virtio_set_status(virtio_device_t *dev, uint8_t status)
{
    dev->status |= status;
    virtio_write32(dev, VIRTIO_MMIO_STATUS, dev->status);
}

uint8_t virtio_get_status(virtio_device_t *dev)
{
    dev->status = virtio_read32(dev, VIRTIO_MMIO_STATUS);
    return dev->status;
}

int virtio_mmio_init(virtio_device_t *dev, uint64_t base_addr, uint32_t device_index)
{
    dev->base_addr = base_addr;
    dev->device_index = device_index;
    dev->num_queues = 0;

    // Check magic value
    uint32_t magic = virtio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC)
    {
        tiny_error("Invalid VirtIO magic value: 0x%x\n", magic);
        return -1;
    }

    // Get version
    dev->version = virtio_read32(dev, VIRTIO_MMIO_VERSION);
    tiny_info("VirtIO version: %d\n", dev->version);

    if (dev->version < 1 || dev->version > 2)
    {
        tiny_error("Unsupported VirtIO version: %d\n", dev->version);
        return -1;
    }

    // Get device and vendor ID
    dev->device_id = virtio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    dev->vendor_id = virtio_read32(dev, VIRTIO_MMIO_VENDOR_ID);

    tiny_info("VirtIO device ID: %d, vendor ID: 0x%x\n", dev->device_id, dev->vendor_id);

    // Reset device
    virtio_write32(dev, VIRTIO_MMIO_STATUS, 0);

    // Acknowledge device
    virtio_set_status(dev, VIRTIO_STATUS_ACKNOWLEDGE);

    // Set driver status
    virtio_set_status(dev, VIRTIO_STATUS_DRIVER);

    return 0;
}

int virtio_queue_setup(virtio_device_t *dev, uint32_t queue_id, uint32_t queue_size)
{
    if (queue_id >= 16)
    {
        tiny_error("Queue ID %d too large\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue = &dev->queues[queue_id];
    queue->queue_id = queue_id;
    queue->queue_size = queue_size;
    queue->last_used_idx = 0;
    queue->free_head = 0;
    queue->num_free = queue_size;

    // Select queue
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, queue_id);

    // Check if queue exists
    uint32_t max_size = virtio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0)
    {
        tiny_error("Queue %d does not exist\n", queue_id);
        return -1;
    }

    if (queue_size > max_size)
    {
        queue_size = max_size;
        queue->queue_size = queue_size;
    }

    // Set queue size
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, queue_size);

    // Memory layout is now handled by device-specific allocation

    // Use device-specific memory allocation
    queue->desc_addr = virtio_get_queue_desc_addr(dev->device_index, queue_id);
    queue->avail_addr = virtio_get_queue_avail_addr(dev->device_index, queue_id);
    queue->used_addr = virtio_get_queue_used_addr(dev->device_index, queue_id);

    if (queue->desc_addr == 0 || queue->avail_addr == 0 || queue->used_addr == 0)
    {
        tiny_error("Failed to get queue memory addresses\n");
        return -1;
    }

    // Set up pointers
    queue->desc = (virtq_desc_t *)queue->desc_addr;
    queue->avail = (virtq_avail_t *)queue->avail_addr;
    queue->used = (virtq_used_t *)queue->used_addr;

    tiny_info("Queue %d memory layout:\n", queue_id);
    tiny_info("  Desc:  0x%lx (0x1000 aligned: %s)\n",
              queue->desc_addr, (queue->desc_addr & 0xFFF) == 0 ? "YES" : "NO");
    tiny_info("  Avail: 0x%lx (offset from desc: 0x%lx)\n",
              queue->avail_addr, queue->avail_addr - queue->desc_addr);
    tiny_info("  Used:  0x%lx (offset from avail: 0x%lx)\n",
              queue->used_addr, queue->used_addr - queue->avail_addr);

    // Initialize descriptor table
    for (uint32_t i = 0; i < queue_size; i++)
    {
        queue->desc[i].addr = 0;
        queue->desc[i].len = 0;
        queue->desc[i].flags = 0;
        queue->desc[i].next = (i + 1) % queue_size;
    }

    // Initialize available ring
    queue->avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT; // No interrupts
    queue->avail->idx = 0;

    // Initialize used ring
    queue->used->flags = 0;
    queue->used->idx = 0;

    // Configure queue addresses based on version
    if (dev->version >= 2)
    {
        // Modern mode
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, queue->desc_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, queue->desc_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, queue->avail_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, queue->avail_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_USED_LOW, queue->used_addr & 0xFFFFFFFF);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_USED_HIGH, queue->used_addr >> 32);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    }
    else
    {
        // Legacy mode
        virtio_write32(dev, VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_ALIGN, 4096);
        virtio_write32(dev, VIRTIO_MMIO_QUEUE_PFN, queue->desc_addr >> 12);
    }

    dev->num_queues = queue_id + 1;

    tiny_info("Queue %d setup complete: size=%d, desc=0x%lx, avail=0x%lx, used=0x%lx\n",
              queue_id, queue_size, queue->desc_addr, queue->avail_addr, queue->used_addr);

    return 0;
}

int virtio_queue_add_buf(virtio_device_t *dev, uint32_t queue_id,
                         uint64_t *buffers, uint32_t *lengths,
                         uint32_t out_num, uint32_t in_num)
{
    if (queue_id >= dev->num_queues)
    {
        tiny_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue = &dev->queues[queue_id];
    uint32_t total_desc = out_num + in_num;

    if (total_desc == 0 || total_desc > queue->num_free)
    {
        tiny_error("Not enough free descriptors: need %d, have %d\n", total_desc, queue->num_free);
        return -1;
    }

    uint16_t head = queue->free_head;
    uint16_t prev = head;

    // Set up output descriptors
    for (uint32_t i = 0; i < out_num; i++)
    {
        queue->desc[prev].addr = buffers[i];
        queue->desc[prev].len = lengths[i];
        queue->desc[prev].flags = (i + 1 < total_desc) ? VIRTQ_DESC_F_NEXT : 0;
        if (i + 1 < total_desc)
        {
            prev = queue->desc[prev].next;
        }
    }

    // Set up input descriptors
    for (uint32_t i = 0; i < in_num; i++)
    {
        queue->desc[prev].addr = buffers[out_num + i];
        queue->desc[prev].len = lengths[out_num + i];
        queue->desc[prev].flags = VIRTQ_DESC_F_WRITE;
        if (i + 1 < in_num)
        {
            queue->desc[prev].flags |= VIRTQ_DESC_F_NEXT;
            prev = queue->desc[prev].next;
        }
    }

    // Update free list
    queue->free_head = queue->desc[prev].next;
    queue->num_free -= total_desc;

    // Add to available ring
    uint16_t avail_idx = queue->avail->idx;
    queue->avail->ring[avail_idx % queue->queue_size] = head;

    // Memory barrier before updating index
    wmb();
    queue->avail->idx = avail_idx + 1;

    return head;
}

int virtio_queue_kick(virtio_device_t *dev, uint32_t queue_id)
{
    if (queue_id >= dev->num_queues)
    {
        tiny_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    // Notify device
    virtio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, queue_id);
    return 0;
}

int virtio_queue_get_buf(virtio_device_t *dev, uint32_t queue_id, uint32_t *len)
{
    if (queue_id >= dev->num_queues)
    {
        tiny_error("Invalid queue ID: %d\n", queue_id);
        return -1;
    }

    virtio_queue_t *queue = &dev->queues[queue_id];

    // Check if there are any used buffers
    rmb();
    if (queue->last_used_idx == queue->used->idx)
    {
        return -1; // No used buffers
    }

    // Get used buffer
    virtq_used_elem_t *used_elem = &queue->used->ring[queue->last_used_idx % queue->queue_size];
    uint32_t desc_id = used_elem->id;
    if (len)
    {
        *len = used_elem->len;
    }

    // Free descriptors
    uint16_t desc_idx = desc_id;
    uint32_t desc_count = 0;

    while (true)
    {
        uint16_t next = queue->desc[desc_idx].next;
        bool has_next = queue->desc[desc_idx].flags & VIRTQ_DESC_F_NEXT;

        // Clear descriptor
        queue->desc[desc_idx].addr = 0;
        queue->desc[desc_idx].len = 0;
        queue->desc[desc_idx].flags = 0;
        queue->desc[desc_idx].next = queue->free_head;

        queue->free_head = desc_idx;
        desc_count++;

        if (!has_next)
        {
            break;
        }
        desc_idx = next;
    }

    queue->num_free += desc_count;
    queue->last_used_idx++;

    return desc_id;
}
