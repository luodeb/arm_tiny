/*
 * virtio_blk.c
 *
 * Created on: 2025-06-19
 * Author: debin
 *
 * Description: VirtIO Block device driver implementation
 */

#include "virtio/virtio_blk.h"
#include "virtio/virtio_mmio.h"
#include "tiny_io.h"
#include "config.h"
#include "virtio/virtio_interrupt.h"

static virtio_blk_config_t blk_config;

// Buffer for block operations - use same memory region as queues for physical address consistency
#define VIRTIO_DATA_BASE 0x45100000 // Just after queue memory
static uint8_t *sector_buffer = (uint8_t *)(VIRTIO_DATA_BASE);
static virtio_blk_req_t *blk_request = (virtio_blk_req_t *)(VIRTIO_DATA_BASE + VIRTIO_BLK_SECTOR_SIZE);
static virtio_device_t blk_dev_global = {0};

virtio_device_t *virtio_get_blk_device(void)
{
    return &blk_dev_global;
}

bool virtio_blk_init(void)
{
    tiny_log(INFO, "[VIRTIO_BLK] Initializing VirtIO Block device\n");

    // Initialize data memory region
    tiny_log(DEBUG, "[VIRTIO_BLK] Initializing data region at 0x%x\n", VIRTIO_DATA_BASE);

    // Clear sector buffer and request structure memory
    for (int i = 0; i < VIRTIO_BLK_SECTOR_SIZE + sizeof(virtio_blk_req_t); i++)
    {
        ((uint8_t *)VIRTIO_DATA_BASE)[i] = 0;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Data region cleared: sector_buffer=0x%p, blk_request=0x%p\n",
             sector_buffer, blk_request);

    // Scan for VirtIO Block device
    uint64_t blk_device_addr = virtio_scan_devices(VIRTIO_DEVICE_ID_BLOCK);
    if (blk_device_addr == 0)
    {
        tiny_log(WARN, "[VIRTIO_BLK] No VirtIO Block device found\n");
        return false;
    }

    tiny_log(INFO, "[VIRTIO_BLK] Found VirtIO Block device at 0x%x\n", (uint32_t)blk_device_addr);

    // Get device from MMIO layer
    virtio_device_t *blk_dev = virtio_get_blk_device();
    if (!virtio_device_init(blk_dev, blk_device_addr))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device initialization FAILED\n");
        return false;
    }

    // Verify this is a block device (double check)
    if (blk_dev->device_id != VIRTIO_DEVICE_ID_BLOCK)
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device ID mismatch: expected %d, got %d\n",
                 VIRTIO_DEVICE_ID_BLOCK, blk_dev->device_id);
        return false;
    }

    tiny_log(INFO, "[VIRTIO_BLK] Block device verified\n");

    // Verify test.img compatibility
    tiny_log(INFO, "[VIRTIO_BLK] Verifying test environment:\n");
    tiny_log(INFO, "[VIRTIO_BLK] - Expected image size: 1048576 bytes (2048 sectors)\n");
    tiny_log(INFO, "[VIRTIO_BLK] - Expected format: FAT32\n");

    // Read device configuration
    uint64_t config_addr = blk_dev->base_addr + 0x100; // Configuration space starts at offset 0x100
    blk_config.capacity = virtio_read32(config_addr) | ((uint64_t)virtio_read32(config_addr + 4) << 32);
    blk_config.size_max = virtio_read32(config_addr + 8);
    blk_config.seg_max = virtio_read32(config_addr + 12);
    blk_config.blk_size = virtio_read32(config_addr + 20);

    tiny_log(INFO, "[VIRTIO_BLK] Device config - Capacity: %d sectors, Block size: %d\n",
             (uint32_t)blk_config.capacity, blk_config.blk_size);

    // Verify device configuration matches test.img
    if (blk_config.capacity != 2048)
    {
        tiny_log(WARN, "[VIRTIO_BLK] WARNING: Device capacity (%d) doesn't match test.img (2048 sectors)\n",
                 (uint32_t)blk_config.capacity);
    }
    if (blk_config.blk_size != 512)
    {
        tiny_log(WARN, "[VIRTIO_BLK] WARNING: Block size (%d) is not standard 512 bytes\n",
                 blk_config.blk_size);
    }

    // Initialize queue manager if not already done
    if (!virtio_queue_manager_init())
    {
        tiny_log(ERROR, "[VIRTIO_BLK] Failed to initialize queue manager\n");
        return false;
    }

    // Allocate a queue for this device
    virtqueue_t *blk_queue = virtio_queue_alloc(blk_dev, 0); // Queue index 0 for block device
    if (!blk_queue)
    {
        tiny_log(ERROR, "[VIRTIO_BLK] Failed to allocate queue\n");
        return false;
    }

    // Initialize the allocated queue
    if (!virtio_queue_init(blk_queue))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Queue initialization FAILED\n");
        virtio_queue_free(blk_queue);
        return false;
    }

    // Set driver OK to complete initialization
    virtio_set_status(blk_dev, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    tiny_log(INFO, "[VIRTIO_BLK] Device ready for operation\n");
    return true;
}

bool test_split(virtqueue_t *blk_queue)
{
    // tiny_log(DEBUG, "[VIRTIO_BLK] Descriptors configured\n");
    uint64_t data = blk_queue->device->base_addr;

    tiny_log(ERROR, "blk_queue %x addr: %x\n", data, blk_queue);

    // virtio_cache_invalidate_range(blk_queue, sizeof(blk_queue));
    //    Submit request
    if (!virtio_queue_submit_request(blk_queue, 0))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to submit request\n");
        return false;
    }
    return true;
}

bool virtio_blk_read_sector(uint32_t sector, void *buffer)
{
    tiny_log(DEBUG, "[VIRTIO_BLK] Reading sector %d\n", sector);

    virtio_device_t *blk_dev = virtio_get_blk_device();
    virtqueue_t *blk_queue = virtio_queue_get_device_queue(blk_dev, 0);

    if (!blk_dev || !blk_queue)
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device not initialized %x %x\n", blk_dev, blk_queue);
        return false;
    }

    // Check device status before operation
    uint32_t device_status = virtio_read32(blk_dev->base_addr + VIRTIO_MMIO_STATUS);
    tiny_log(DEBUG, "[VIRTIO_BLK] Device status before operation: 0x%x\n", device_status);

    if (!(device_status & VIRTIO_STATUS_DRIVER_OK))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device not ready! Status: 0x%x\n", device_status);
        return false;
    }

    // Clear request structure completely
    for (int i = 0; i < sizeof(virtio_blk_req_t); i++)
    {
        ((uint8_t *)blk_request)[i] = 0;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Request structure cleared\n");

    // Debug: Check structure alignment and address
    tiny_log(DEBUG, "[VIRTIO_BLK] blk_request address: 0x%p\n", blk_request);
    tiny_log(DEBUG, "[VIRTIO_BLK] blk_request->header address: 0x%p\n", &blk_request->header);
    tiny_log(DEBUG, "[VIRTIO_BLK] blk_request->header.sector address: 0x%p\n", &blk_request->header.sector);
    tiny_log(DEBUG, "[VIRTIO_BLK] Structure size: %d bytes\n", (int)sizeof(virtio_blk_req_t));

    tiny_log(DEBUG, "[VIRTIO_BLK] Queue size: %d\n", blk_queue->queue_size);

    // Clear buffers
    for (int i = 0; i < VIRTIO_BLK_SECTOR_SIZE; i++)
    {
        sector_buffer[i] = 0;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Sector buffer cleared\n");

    // Setup request header - set values before reading
    blk_request->header.type = VIRTIO_BLK_T_IN; // Read operation
    blk_request->header.reserved = 0;
    blk_request->header.sector = sector;
    blk_request->status = 0xFF; // Set to non-zero to detect completion

    tiny_log(DEBUG, "[VIRTIO_BLK] Request header configured - Type: %d, Sector: %d, Status: 0x%x\n",
             blk_request->header.type, (uint32_t)blk_request->header.sector, blk_request->status);

    // Setup descriptors
    // Descriptor 0: Request header (device read)
    if (!virtio_queue_add_descriptor(blk_queue, 0, (uint64_t)&blk_request->header,
                                     sizeof(virtio_blk_req_header_t), VIRTQ_DESC_F_NEXT, 1))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add header descriptor\n");
        return false;
    }

    // Descriptor 1: Data buffer (device write)
    if (!virtio_queue_add_descriptor(blk_queue, 1, (uint64_t)sector_buffer,
                                     VIRTIO_BLK_SECTOR_SIZE,
                                     VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT, 2))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add data descriptor\n");
        return false;
    }

    // Descriptor 2: Status byte (device write)
    if (!virtio_queue_add_descriptor(blk_queue, 2, (uint64_t)&blk_request->status,
                                     1, VIRTQ_DESC_F_WRITE, 0))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add status descriptor\n");
        return false;
    }

    if (!test_split(blk_queue))
    {
        return false;
    }

    // Wait for completion
#if USE_VIRTIO_IRQ
    if (!virtio_wait_for_interrupt(VIRTIO_IRQ_TIMEOUT_MS))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Request timeout (interrupt not received)\n");
        return false;
    }
#else
    // Polling mode - wait for completion
    if (!virtio_queue_wait_for_completion(blk_queue))
    {
        tiny_log(ERROR, "[VIRTIO_BLK] Request timeout\n");
        return false;
    }
#endif

    // Invalidate data buffer cache after device writes
    virtio_cache_invalidate_range((uint64_t)sector_buffer, VIRTIO_BLK_SECTOR_SIZE);
    virtio_cache_invalidate_range((uint64_t)&blk_request->status, sizeof(blk_request->status));

    // Check status
    if (blk_request->status != VIRTIO_BLK_S_OK)
    {
        tiny_log(WARN, "[VIRTIO_BLK] Request failed with status: %d\n", blk_request->status);
        return false;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Read completed successfully\n");

    // Copy data to output buffer
    for (int i = 0; i < VIRTIO_BLK_SECTOR_SIZE; i++)
    {
        ((uint8_t *)buffer)[i] = sector_buffer[i];
    }

    tiny_log(INFO, "[VIRTIO_BLK] Sector %d read SUCCESSFUL\n", sector);
    return true;
}

bool virtio_blk_write_sector(uint32_t sector, const void *buffer)
{
    tiny_log(DEBUG, "[VIRTIO_BLK] Writing sector %d\n", sector);

    virtio_device_t *blk_dev = virtio_get_blk_device();
    virtqueue_t *blk_queue = virtio_queue_get_device_queue(blk_dev, 0);

    if (!blk_dev || !blk_queue)
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device not initialized\n");
        return false;
    }

    tiny_log(ERROR, "blk_queue %x\n", blk_queue->device->base_addr);

    // Check device status before operation
    uint32_t device_status = virtio_read32(blk_dev->base_addr + VIRTIO_MMIO_STATUS);
    tiny_log(DEBUG, "[VIRTIO_BLK] Device status before operation: 0x%x\n", device_status);

    if (!(device_status & VIRTIO_STATUS_DRIVER_OK))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Device not ready! Status: 0x%x\n", device_status);
        return false;
    }

    // Clear request structure completely
    for (int i = 0; i < sizeof(virtio_blk_req_t); i++)
    {
        ((uint8_t *)blk_request)[i] = 0;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Request structure cleared\n");

    // Copy input data to sector buffer
    for (int i = 0; i < VIRTIO_BLK_SECTOR_SIZE; i++)
    {
        sector_buffer[i] = ((const uint8_t *)buffer)[i];
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Data copied to sector buffer\n");

    // Setup request header - set values before writing
    blk_request->header.type = VIRTIO_BLK_T_OUT; // Write operation
    blk_request->header.reserved = 0;
    blk_request->header.sector = sector;
    blk_request->status = 0xFF; // Set to non-zero to detect completion

    tiny_log(DEBUG, "[VIRTIO_BLK] Request header configured - Type: %d, Sector: %d, Status: 0x%x\n",
             blk_request->header.type, (uint32_t)blk_request->header.sector, blk_request->status);

    // Clean data buffer cache before device reads
    virtio_cache_clean_range((uint64_t)sector_buffer, VIRTIO_BLK_SECTOR_SIZE);
    virtio_cache_clean_range((uint64_t)&blk_request->header, sizeof(virtio_blk_req_header_t));

    // Setup descriptors
    // Descriptor 0: Request header (device read)
    if (!virtio_queue_add_descriptor(blk_queue, 0, (uint64_t)&blk_request->header,
                                     sizeof(virtio_blk_req_header_t), VIRTQ_DESC_F_NEXT, 1))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add header descriptor\n");
        return false;
    }

    // Descriptor 1: Data buffer (device read for write operation)
    if (!virtio_queue_add_descriptor(blk_queue, 1, (uint64_t)sector_buffer,
                                     VIRTIO_BLK_SECTOR_SIZE, VIRTQ_DESC_F_NEXT, 2))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add data descriptor\n");
        return false;
    }

    // Descriptor 2: Status byte (device write)
    if (!virtio_queue_add_descriptor(blk_queue, 2, (uint64_t)&blk_request->status,
                                     1, VIRTQ_DESC_F_WRITE, 0))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to add status descriptor\n");
        return false;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Descriptors configured\n");
    tiny_log(ERROR, "blk_queue %x", blk_queue->device->base_addr);

    // Submit request
    if (!virtio_queue_submit_request(blk_queue, 0))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Failed to submit request\n");
        return false;
    }

    // Wait for completion
#if USE_VIRTIO_IRQ
    if (!virtio_wait_for_interrupt(VIRTIO_IRQ_TIMEOUT_MS))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Request timeout (interrupt not received)\n");
        return false;
    }
#else
    // Polling mode - wait for completion
    if (!virtio_queue_wait_for_completion(blk_queue))
    {
        tiny_log(ERROR, "[VIRTIO_BLK] Request timeout\n");
        return false;
    }
#endif
    tiny_log(DEBUG, "[VIRTIO_BLK] Write completed successfully\n");

    // Invalidate status cache after device writes
    virtio_cache_invalidate_range((uint64_t)&blk_request->status, sizeof(blk_request->status));

    // Check status
    if (blk_request->status != VIRTIO_BLK_S_OK)
    {
        tiny_log(WARN, "[VIRTIO_BLK] Write request failed with status: %d\n", blk_request->status);
        return false;
    }

    tiny_log(DEBUG, "[VIRTIO_BLK] Write completed successfully\n");
    tiny_log(INFO, "[VIRTIO_BLK] Sector %d write SUCCESSFUL\n", sector);
    return true;
}

uint64_t virtio_blk_get_capacity(void)
{
    virtio_device_t *blk_dev = virtio_get_blk_device();
    if (!blk_dev)
    {
        return 0;
    }
    return blk_config.capacity;
}

bool virtio_blk_test(void)
{
    tiny_log(INFO, "[VIRTIO_BLK] Running block device test\n");

    if (!virtio_blk_init())
    {
        tiny_log(WARN, "[VIRTIO_BLK] Test FAILED - initialization error\n");
        return false;
    }

    uint64_t capacity = virtio_blk_get_capacity();
    tiny_log(INFO, "[VIRTIO_BLK] Device capacity: %d sectors\n", (uint32_t)capacity);

    // Test reading sector 0 (boot sector)
    uint8_t test_buffer[VIRTIO_BLK_SECTOR_SIZE];
    if (!virtio_blk_read_sector(0, test_buffer))
    {
        tiny_log(WARN, "[VIRTIO_BLK] Test FAILED - sector read error\n");
        return false;
    }

    tiny_log(INFO, "[VIRTIO_BLK] Test SUCCESSFUL - first 16 bytes: ");
    for (int i = 0; i < 16; i++)
    {
        tiny_log(NONE, "%x ", test_buffer[i]);
    }
    tiny_log(NONE, "\n");

    return true;
}
