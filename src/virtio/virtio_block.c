#include "virtio/virtio_block.h"
#include "tinystd.h"
#include "tinyio.h"

void virtio_blk_get_config(virtio_blk_device_t *blk_dev)
{
    virtio_device_t *dev = blk_dev->dev;

    // Read configuration from device
    uint64_t config_base = dev->base_addr + VIRTIO_MMIO_CONFIG;

    blk_dev->config.capacity = read64((volatile void *)config_base);
    blk_dev->config.size_max = read32((volatile void *)(config_base + 8));
    blk_dev->config.seg_max = read32((volatile void *)(config_base + 12));
    blk_dev->config.blk_size = read32((volatile void *)(config_base + 20));

    // Set defaults if not provided
    blk_dev->block_size = blk_dev->config.blk_size ? blk_dev->config.blk_size : 512;
    blk_dev->capacity = blk_dev->config.capacity;

    tiny_info("Block device config: capacity=%llu, block_size=%u\n",
              blk_dev->capacity, blk_dev->block_size);
}

int virtio_blk_init(virtio_blk_device_t *blk_dev, uint64_t base_addr, uint32_t device_index)
{
    // Initialize VirtIO allocator first
    if (virtio_allocator_init() < 0)
    {
        tiny_error("Failed to initialize VirtIO allocator\n");
        return -1;
    }

    // Allocate device structure
    blk_dev->dev = (virtio_device_t *)virtio_alloc(sizeof(virtio_device_t), 8);
    if (!blk_dev->dev)
    {
        tiny_error("Failed to allocate device structure\n");
        return -1;
    }

    // Initialize VirtIO device
    if (virtio_mmio_init(blk_dev->dev, base_addr, device_index) < 0)
    {
        tiny_error("Failed to initialize VirtIO device\n");
        return -1;
    }

    // Check if this is a block device
    if (blk_dev->dev->device_id != VIRTIO_ID_BLOCK)
    {
        tiny_error("Device is not a block device (ID: %d)\n", blk_dev->dev->device_id);
        return -1;
    }

    // Read device features
    virtio_write32(blk_dev->dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    blk_dev->dev->device_features = virtio_read32(blk_dev->dev, VIRTIO_MMIO_DEVICE_FEATURES);

    tiny_info("Block device features: 0x%lx\n", blk_dev->dev->device_features);

    // Set driver features (we don't need any special features for basic operation)
    blk_dev->dev->driver_features = 0;
    virtio_write32(blk_dev->dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virtio_write32(blk_dev->dev, VIRTIO_MMIO_DRIVER_FEATURES, blk_dev->dev->driver_features);

    // Set features OK
    virtio_set_status(blk_dev->dev, VIRTIO_STATUS_FEATURES_OK);

    // Check if device accepted our features
    if (!(virtio_get_status(blk_dev->dev) & VIRTIO_STATUS_FEATURES_OK))
    {
        tiny_error("Device rejected our features\n");
        return -1;
    }

    // Setup queue (block devices typically use queue 0)
    if (virtio_queue_setup(blk_dev->dev, 0, 16) < 0)
    {
        tiny_error("Failed to setup queue\n");
        return -1;
    }

    // Set driver OK
    virtio_set_status(blk_dev->dev, VIRTIO_STATUS_DRIVER_OK);

    // Read device configuration
    virtio_blk_get_config(blk_dev);

    tiny_info("VirtIO block device initialized successfully\n");
    return 0;
}

int virtio_blk_read_sector(virtio_blk_device_t *blk_dev, uint64_t sector,
                           void *buffer, uint32_t count)
{
    if (!blk_dev || !blk_dev->dev || !buffer)
    {
        tiny_error("Invalid parameters\n");
        return -1;
    }

    uint32_t sector_size = blk_dev->block_size;
    uint32_t total_size = count * sector_size;

    // Allocate request structure
    virtio_blk_req_t *req = (virtio_blk_req_t *)virtio_alloc(sizeof(virtio_blk_req_t), 8);
    uint8_t *status = (uint8_t *)virtio_alloc(1, 1);

    // Setup request
    req->type = VIRTIO_BLK_T_IN; // Read operation
    req->reserved = 0;
    req->sector = sector;

    // Setup buffer arrays for virtqueue
    uint64_t buffers[3];
    uint32_t lengths[3];

    buffers[0] = (uint64_t)req;
    lengths[0] = sizeof(virtio_blk_req_t);

    buffers[1] = (uint64_t)buffer;
    lengths[1] = total_size;

    buffers[2] = (uint64_t)status;
    lengths[2] = 1;

    tiny_debug("Reading sector %llu, count %u, total_size %u\n", sector, count, total_size);

    // Add buffer to queue (1 out, 2 in)
    int desc_id = virtio_queue_add_buf(blk_dev->dev, 0, buffers, lengths, 1, 2);
    if (desc_id < 0)
    {
        tiny_error("Failed to add buffer to queue\n");
        return -1;
    }

    // Kick the queue
    if (virtio_queue_kick(blk_dev->dev, 0) < 0)
    {
        tiny_error("Failed to kick queue\n");
        return -1;
    }

    // Poll for completion (NO_INTERRUPT mode)
    uint32_t timeout = 1000000; // Large timeout
    uint32_t len;
    int result = -1;

    while (timeout-- > 0)
    {
        result = virtio_queue_get_buf(blk_dev->dev, 0, &len);
        if (result >= 0)
        {
            break;
        }
        // Small delay
        for (volatile int i = 0; i < 100; i++)
            ;
    }

    if (result < 0)
    {
        tiny_error("Timeout waiting for block read completion\n");
        return -1;
    }

    // Check status
    if (*status != VIRTIO_BLK_S_OK)
    {
        tiny_error("Block read failed with status: %d\n", *status);
        return -1;
    }

    tiny_debug("Block read completed successfully, len=%u\n", len);
    return 0;
}

int virtio_blk_write_sector(virtio_blk_device_t *blk_dev, uint64_t sector,
                            const void *buffer, uint32_t count)
{
    if (!blk_dev || !blk_dev->dev || !buffer)
    {
        tiny_error("Invalid parameters\n");
        return -1;
    }

    uint32_t sector_size = blk_dev->block_size;
    uint32_t total_size = count * sector_size;

    // Allocate request structure
    virtio_blk_req_t *req = (virtio_blk_req_t *)virtio_alloc(sizeof(virtio_blk_req_t), 8);
    uint8_t *status = (uint8_t *)virtio_alloc(1, 1);

    // Setup request
    req->type = VIRTIO_BLK_T_OUT; // Write operation
    req->reserved = 0;
    req->sector = sector;

    // Setup buffer arrays for virtqueue
    uint64_t buffers[3];
    uint32_t lengths[3];

    buffers[0] = (uint64_t)req;
    lengths[0] = sizeof(virtio_blk_req_t);

    buffers[1] = (uint64_t)buffer;
    lengths[1] = total_size;

    buffers[2] = (uint64_t)status;
    lengths[2] = 1;

    tiny_debug("Writing sector %llu, count %u, total_size %u\n", sector, count, total_size);

    // Add buffer to queue (2 out, 1 in)
    int desc_id = virtio_queue_add_buf(blk_dev->dev, 0, buffers, lengths, 2, 1);
    if (desc_id < 0)
    {
        tiny_error("Failed to add buffer to queue\n");
        return -1;
    }

    // Kick the queue
    if (virtio_queue_kick(blk_dev->dev, 0) < 0)
    {
        tiny_error("Failed to kick queue\n");
        return -1;
    }

    // Poll for completion (NO_INTERRUPT mode)
    uint32_t timeout = 1000000; // Large timeout
    uint32_t len;
    int result = -1;

    while (timeout-- > 0)
    {
        result = virtio_queue_get_buf(blk_dev->dev, 0, &len);
        if (result >= 0)
        {
            break;
        }
        // Small delay
        for (volatile int i = 0; i < 100; i++)
            ;
    }

    if (result < 0)
    {
        tiny_error("Timeout waiting for block write completion\n");
        return -1;
    }

    // Check status
    if (*status != VIRTIO_BLK_S_OK)
    {
        tiny_error("Block write failed with status: %d\n", *status);
        return -1;
    }

    tiny_debug("Block write completed successfully, len=%u\n", len);
    return 0;
}
