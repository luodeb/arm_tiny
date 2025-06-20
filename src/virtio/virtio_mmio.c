/*
 * virtio_mmio.c
 *
 * Created on: 2025-06-19
 * Author: debin
 *
 * Description: VirtIO MMIO device driver implementation
 */

#include "virtio/virtio_mmio.h"
#include "tiny_io.h"

// ARM cache management functions for DMA coherency
void virtio_cache_clean_range(uint64_t start, uint32_t size)
{
    uint64_t end = start + size;
    uint64_t line_size = 64; // ARM cache line size

    tiny_printf(DEBUG, "[VIRTIO] Cache clean: addr=0x%x, size=%d\n", (uint32_t)start, size);

    // Align to cache line boundaries
    start = start & ~(line_size - 1);
    end = (end + line_size - 1) & ~(line_size - 1);

    for (uint64_t addr = start; addr < end; addr += line_size)
    {
        __asm__ volatile("dc cvac, %0" : : "r"(addr) : "memory");
    }

    // Data memory barrier
    __asm__ volatile("dmb sy" ::: "memory");
}

void virtio_cache_invalidate_range(uint64_t start, uint32_t size)
{
    uint64_t end = start + size;
    uint64_t line_size = 64; // ARM cache line size

    // tiny_printf(DEBUG, "[VIRTIO] Cache invalidate: addr=0x%x, size=%d\n", (uint32_t)start, size);

    // Align to cache line boundaries
    start = start & ~(line_size - 1);
    end = (end + line_size - 1) & ~(line_size - 1);

    for (uint64_t addr = start; addr < end; addr += line_size)
    {
        __asm__ volatile("dc ivac, %0" : : "r"(addr) : "memory");
    }

    // Data memory barrier
    __asm__ volatile("dmb sy" ::: "memory");
}

static virtio_device_t virtio_dev;
static virtqueue_t virtio_queue;

uint32_t virtio_read32(uint64_t addr)
{
    uint32_t value = read32((void *)addr);
    tiny_printf(DEBUG, "[VIRTIO] READ32: addr=0x%x, value=0x%x\n", (uint32_t)addr, value);
    return value;
}

void virtio_write32(uint64_t addr, uint32_t value)
{
    tiny_printf(DEBUG, "[VIRTIO] WRITE32: addr=0x%x, value=0x%x\n", (uint32_t)addr, value);
    write32(value, (void *)addr);
}

bool virtio_probe_device(uint64_t base_addr)
{
    tiny_printf(INFO, "[VIRTIO] Probing device at address 0x%x\n", (uint32_t)base_addr);

    // Read magic number
    uint32_t magic = virtio_read32(base_addr + VIRTIO_MMIO_MAGIC);
    if (magic != VIRTIO_MAGIC_VALUE)
    {
        tiny_printf(WARN, "[VIRTIO] Invalid magic number: expected 0x%x, got 0x%x\n",
                    VIRTIO_MAGIC_VALUE, magic);
        return false;
    }

    tiny_printf(INFO, "[VIRTIO] Magic number check PASSED: 0x%x\n", magic);

    // Read version
    uint32_t version = virtio_read32(base_addr + VIRTIO_MMIO_VERSION);
    if (version != 1)
    {
        tiny_printf(WARN, "[VIRTIO] Unsupported version: %d\n", version);
        return false;
    }

    tiny_printf(INFO, "[VIRTIO] Version check PASSED: %d\n", version);

    // Read device ID
    uint32_t device_id = virtio_read32(base_addr + VIRTIO_MMIO_DEVICE_ID);
    if (device_id == 0)
    {
        tiny_printf(WARN, "[VIRTIO] No device present (device_id = 0)\n");
        return false;
    }

    tiny_printf(INFO, "[VIRTIO] Device ID: %d\n", device_id);

    return true;
}

bool virtio_device_init(virtio_device_t *dev, uint64_t base_addr)
{
    tiny_printf(INFO, "[VIRTIO] Initializing device at 0x%x\n", (uint32_t)base_addr);

    // Probe device first
    if (!virtio_probe_device(base_addr))
    {
        tiny_printf(WARN, "[VIRTIO] Device probe FAILED\n");
        return false;
    }

    // Fill device structure
    dev->base_addr = base_addr;
    dev->magic = virtio_read32(base_addr + VIRTIO_MMIO_MAGIC);
    dev->version = virtio_read32(base_addr + VIRTIO_MMIO_VERSION);
    dev->device_id = virtio_read32(base_addr + VIRTIO_MMIO_DEVICE_ID);
    dev->vendor_id = virtio_read32(base_addr + VIRTIO_MMIO_VENDOR_ID);

    tiny_printf(INFO, "[VIRTIO] Device info - Magic: 0x%x, Version: %d, Device ID: %d, Vendor ID: 0x%x\n",
                dev->magic, dev->version, dev->device_id, dev->vendor_id);

    // Note: QEMU's virtio-blk-device only supports Legacy mode (version 1)
    // Modern mode (version 2+) is not available for virtio-blk-device
    if (dev->version == 1)
    {
        tiny_printf(INFO, "[VIRTIO] Device uses VirtIO 1.0 Legacy mode (expected for virtio-blk-device)\n");
    }
    else if (dev->version >= 2)
    {
        tiny_printf(INFO, "[VIRTIO] Device uses VirtIO 1.1+ Modern mode (version %d)\n", dev->version);
    }
    else
    {
        tiny_printf(WARN, "[VIRTIO] Unknown VirtIO version: %d\n", dev->version);
    }

    // Reset device
    virtio_set_status(dev, 0);
    tiny_printf(INFO, "[VIRTIO] Device reset completed\n");

    // Acknowledge the device
    virtio_set_status(dev, VIRTIO_STATUS_ACKNOWLEDGE);
    tiny_printf(INFO, "[VIRTIO] Device acknowledged\n");

    // Indicate that we know how to drive the device
    virtio_set_status(dev, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    tiny_printf(INFO, "[VIRTIO] Driver status set\n");

    // Read device features
    uint32_t device_features = virtio_read32(dev->base_addr + VIRTIO_MMIO_DEVICE_FEATURES);
    tiny_printf(INFO, "[VIRTIO] Device features: 0x%x\n", device_features);

    // For simplicity, accept all features
    virtio_write32(dev->base_addr + VIRTIO_MMIO_DRIVER_FEATURES, device_features);
    tiny_printf(INFO, "[VIRTIO] Driver features set to: 0x%x\n", device_features);

    // Indicate that feature negotiation is complete
    virtio_set_status(dev, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    tiny_printf(INFO, "[VIRTIO] Features OK status set\n");

    // Check if device accepted our features
    uint8_t status = virtio_read32(dev->base_addr + VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK))
    {
        tiny_printf(ERROR, "[VIRTIO] Device rejected our features\n");
        return false;
    }

    dev->ready = false;

    tiny_printf(INFO, "[VIRTIO] Device initialization SUCCESSFUL\n");
    return true;
}

void virtio_set_status(virtio_device_t *dev, uint8_t status)
{
    // Read back to verify
    uint8_t current_status = virtio_read32(dev->base_addr + VIRTIO_MMIO_STATUS);
    tiny_printf(DEBUG, "[VIRTIO] Status readback: 0x%x\n", current_status);

    tiny_printf(DEBUG, "[VIRTIO] Setting device status to 0x%x\n", status);
    virtio_write32(dev->base_addr + VIRTIO_MMIO_STATUS, status);

    // Read back to verify
    current_status = virtio_read32(dev->base_addr + VIRTIO_MMIO_STATUS);
    tiny_printf(DEBUG, "[VIRTIO] Status readback: 0x%x\n", current_status);
}

bool virtio_queue_init(virtio_device_t *dev, uint32_t queue_idx)
{
    tiny_printf(INFO, "[VIRTIO] Initializing queue %d\n", queue_idx);

    // Select queue
    virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_SEL, queue_idx);

    // Get maximum queue size
    uint32_t queue_num_max = virtio_read32(dev->base_addr + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_num_max == 0)
    {
        tiny_printf(WARN, "[VIRTIO] Queue %d not available\n", queue_idx);
        return false;
    }

    tiny_printf(INFO, "[VIRTIO] Queue %d max size: %d\n", queue_idx, queue_num_max);
    dev->queue_num_max = queue_num_max;

    // Set queue size with safety limit - use very small size for testing
    uint32_t queue_size = queue_num_max;
    if (queue_size > 16)
    {
        queue_size = 16; // Use very small queue size for debugging
        tiny_printf(INFO, "[VIRTIO] Queue size limited to 16 for debugging\n");
    }
    virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_NUM, queue_size);
    tiny_printf(INFO, "[VIRTIO] Queue %d size set to: %d\n", queue_idx, queue_size);

    // VirtIO 1.0 Legacy mode requires all queue components in a single contiguous memory region
    // starting from PFN-specified page. Modern mode uses separate addresses.

    // Calculate sizes for all components
    uint64_t desc_size = queue_size * sizeof(virtq_desc_t); // queue_size * 16
    uint64_t avail_size = 6 + (queue_size * 2);             // 6 + queue_size * 2
    uint64_t used_size = 6 + (queue_size * 2);              // 6 + queue_size * 2

    // Base address must be 4KB aligned for PFN calculation
    uint64_t base_addr = 0x45000000; // 4KB-aligned base address

    // Legacy mode layout (all components in one contiguous region)
    uint64_t desc_addr = base_addr;               // Descriptor table (16-byte aligned)
    uint64_t avail_addr = desc_addr + desc_size;  // Available ring follows descriptors
    uint64_t used_addr = (avail_addr + used_size + 4096 - 1) & ~(4096 - 1);

    // Verify total size fits in reasonable bounds (should be < 4KB for small queues)
    uint64_t total_size = used_addr + used_size - base_addr;

    tiny_printf(INFO, "[VIRTIO] Legacy layout - Base: 0x%x, Desc: 0x%x, Avail: 0x%x, Used: 0x%x\n",
                (uint32_t)base_addr, (uint32_t)desc_addr, (uint32_t)avail_addr, (uint32_t)used_addr);
    tiny_printf(INFO, "[VIRTIO] Component sizes - Desc: %d, Avail: %d, Used: %d, Total: %d bytes\n",
                (int)desc_size, (int)avail_size, (int)used_size, (int)total_size);
    tiny_printf(INFO, "[VIRTIO] Address offsets - Avail: +%d, Used: +%d\n",
                (int)(avail_addr - base_addr), (int)(used_addr - base_addr));

    if (total_size > 8192)
    {
        tiny_printf(WARN, "[VIRTIO] Queue layout exceeds 8KB, this may cause issues\n");
    }
    else
    {
        tiny_printf(INFO, "[VIRTIO] Legacy queue layout validation PASSED (total %d bytes)\n", (int)total_size);
    }

    // Clear entire queue memory region safely
    volatile uint8_t *base_ptr = (volatile uint8_t *)base_addr;

    tiny_printf(DEBUG, "[VIRTIO] Clearing entire queue region (%d bytes) from 0x%x...\n",
                (int)total_size, (uint32_t)base_addr);

    // Clear entire memory region byte by byte to avoid crashes
    for (int i = 0; i < total_size; i++)
    {
        base_ptr[i] = 0;
    }

    tiny_printf(DEBUG, "[VIRTIO] All queue memory cleared successfully\n");

    // Check VirtIO version for compatibility - prefer Modern mode
    if (dev->version >= 2)
    {
        // VirtIO 1.1+ modern interface
        tiny_printf(INFO, "[VIRTIO] Using VirtIO 1.1+ modern interface (queue_size=%d)\n", queue_size);

        // Set queue addresses with improved layout
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)desc_addr);
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(desc_addr >> 32));

        // Add memory barrier after descriptor table setup
        __asm__ volatile("dmb sy" ::: "memory");

        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)avail_addr);
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(avail_addr >> 32));

        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)used_addr);
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(used_addr >> 32));

        // Add memory barrier before enabling queue
        __asm__ volatile("dmb sy" ::: "memory");

        // Enable the queue
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_READY, 1);

        // Verify queue is ready
        uint32_t queue_ready = virtio_read32(dev->base_addr + VIRTIO_MMIO_QUEUE_READY);
        if (queue_ready != 1)
        {
            tiny_printf(WARN, "[VIRTIO] Queue %d failed to become ready (ready=%d)\n", queue_idx, queue_ready);
            return false;
        }

        tiny_printf(INFO, "[VIRTIO] Modern mode queue %d successfully activated\n", queue_idx);
    }
    else
    {
        // VirtIO 1.0 legacy interface - all components in contiguous memory
        tiny_printf(INFO, "[VIRTIO] Using VirtIO 1.0 legacy interface (queue_size=%d)\n", queue_size);

        // Add memory barrier before setting page size
        __asm__ volatile("dmb sy" ::: "memory");

        // Set guest page size (4KB) - required for PFN calculation
        virtio_write32(dev->base_addr + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

        // CRITICAL: Set queue alignment - this is required in Legacy mode
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_ALIGN, 4096);
        tiny_printf(INFO, "[VIRTIO] Queue alignment set to 4096 bytes\n");

        // Set queue PFN (physical frame number) - device calculates component addresses automatically
        uint32_t queue_pfn = (uint32_t)(base_addr >> 12); // Use base_addr instead of desc_addr
        virtio_write32(dev->base_addr + VIRTIO_MMIO_QUEUE_PFN, queue_pfn);

        // CRITICAL: Add stronger memory barriers and cache operations after PFN setup
        __asm__ volatile("dmb sy" ::: "memory"); // Data memory barrier
        __asm__ volatile("dsb sy" ::: "memory"); // Data synchronization barrier
        __asm__ volatile("isb" ::: "memory");    // Instruction synchronization barrier

        // Additional cache cleaning to ensure device sees the configuration
        virtio_cache_clean_range(base_addr, total_size);

        tiny_printf(INFO, "[VIRTIO] Legacy mode queue PFN set to: 0x%x (base_addr=0x%x)\n",
                    queue_pfn, (uint32_t)base_addr);
        tiny_printf(INFO, "[VIRTIO] Device will auto-calculate: Desc=0x%x, Avail=0x%x, Used=0x%x\n",
                    (uint32_t)desc_addr, (uint32_t)avail_addr, (uint32_t)used_addr);

        // Verify PFN calculation is correct
        uint32_t pfn_check = virtio_read32(dev->base_addr + VIRTIO_MMIO_QUEUE_PFN);
        if (pfn_check != queue_pfn)
        {
            tiny_printf(WARN, "[VIRTIO] PFN readback mismatch: wrote 0x%x, read 0x%x\n", queue_pfn, pfn_check);
        }
        else
        {
            tiny_printf(INFO, "[VIRTIO] PFN readback verification PASSED: 0x%x\n", pfn_check);
        }
    }

    // Initialize queue structure - let device calculate the actual layout according to VirtIO spec
    virtio_queue.queue_size = queue_size;
    virtio_queue.desc_table_addr = base_addr; // Descriptor table starts at PFN address

    virtio_queue.avail_ring_addr = avail_addr;
    virtio_queue.used_ring_addr = used_addr;
    virtio_queue.last_used_idx = 0;

    // Set up pointers to device-calculated addresses
    virtio_queue.desc = (virtq_desc_t *)base_addr;
    virtio_queue.avail = (virtq_avail_t *)avail_addr;
    virtio_queue.used = (virtq_used_t *)used_addr;

    tiny_printf(INFO, "[VIRTIO] Device-calculated addresses - Desc: 0x%x, Avail: 0x%x, Used: 0x%x\n",
                (uint32_t)base_addr, (uint32_t)avail_addr, (uint32_t)used_addr);

    // CRITICAL: Ensure all queue memory is properly flushed to main memory
    // Clean entire queue region to ensure device sees initialized memory
    virtio_cache_clean_range(base_addr, total_size);

    // Strong memory barrier to ensure all setup is complete before device activation
    __asm__ volatile("dmb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory"); // Instruction synchronization barrier

    // CRITICAL: Set VIRTQ_AVAIL_F_NO_INTERRUPT flag to enable polling mode
    // This tells the device NOT to use interrupts and allows pure polling
    virtio_queue.avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    tiny_printf(INFO, "[VIRTIO] Set avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT (0x%x) for polling mode\n",
                VIRTQ_AVAIL_F_NO_INTERRUPT);

    // Ensure the flag setting is visible to the device
    virtio_cache_clean_range((uint64_t)virtio_queue.avail, sizeof(virtq_avail_t));
    __asm__ volatile("dmb sy" ::: "memory");

    tiny_printf(INFO, "[VIRTIO] Queue %d initialization SUCCESSFUL (polling mode enabled)\n", queue_idx);
    return true;
}

virtio_device_t *virtio_get_device(void)
{
    return &virtio_dev;
}

virtqueue_t *virtio_get_queue(void)
{
    return &virtio_queue;
}

// Queue management functions
bool virtio_queue_add_descriptor(uint16_t desc_idx, uint64_t addr, uint32_t len, uint16_t flags, uint16_t next)
{
    if (desc_idx >= virtio_queue.queue_size)
    {
        tiny_printf(WARN, "[VIRTIO] Invalid descriptor index: %d\n", desc_idx);
        return false;
    }

    virtio_queue.desc[desc_idx].addr = addr;
    virtio_queue.desc[desc_idx].len = len;
    virtio_queue.desc[desc_idx].flags = flags;
    virtio_queue.desc[desc_idx].next = next;

    tiny_printf(DEBUG, "[VIRTIO] Added descriptor %d: addr=0x%x, len=%d, flags=0x%x\n",
                desc_idx, (uint32_t)addr, len, flags);

    return true;
}

bool virtio_queue_submit_request(uint16_t desc_head)
{
    // Cache management: Clean descriptor table and data buffers before submission
    virtio_cache_clean_range((uint64_t)virtio_queue.desc,
                             virtio_queue.queue_size * sizeof(virtq_desc_t));

    // Clean the specific descriptors in the chain
    uint16_t current_desc = desc_head;
    while (current_desc < virtio_queue.queue_size)
    {
        virtq_desc_t *desc = &virtio_queue.desc[current_desc];

        // Clean the data buffer pointed to by this descriptor
        virtio_cache_clean_range(desc->addr, desc->len);

        tiny_printf(DEBUG, "[VIRTIO] Cleaned descriptor %d buffer: addr=0x%x, len=%d\n",
                    current_desc, (uint32_t)desc->addr, desc->len);

        if (!(desc->flags & VIRTQ_DESC_F_NEXT))
        {
            break;
        }
        current_desc = desc->next;
    }

    // Add to available ring
    uint16_t avail_idx = virtio_queue.avail->idx;
    virtio_queue.avail->ring[avail_idx % virtio_queue.queue_size] = desc_head;

    // CRITICAL: Ensure NO_INTERRUPT flag is set for polling mode
    virtio_queue.avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;

    // Memory barrier - critical for ARM architecture
    __asm__ volatile("dmb sy" ::: "memory");

    // Update available index
    virtio_queue.avail->idx = avail_idx + 1;

    tiny_printf(DEBUG, "[VIRTIO] Request queued: desc_head=%d, avail_idx=%d->%d, flags=0x%x\n",
                desc_head, avail_idx, avail_idx + 1, virtio_queue.avail->flags);

    // Clean available ring to ensure device can see the update
    virtio_cache_clean_range((uint64_t)virtio_queue.avail,
                             sizeof(virtq_avail_t) + virtio_queue.queue_size * sizeof(uint16_t));

    // Another memory barrier to ensure index update is visible
    __asm__ volatile("dmb sy" ::: "memory");

    tiny_printf(DEBUG, "[VIRTIO] Submitted request: desc_head=%d, avail_idx=%d\n",
                desc_head, avail_idx);

    // Notify device - queue index should be passed
    virtio_write32(virtio_dev.base_addr + VIRTIO_MMIO_QUEUE_NOTIFY, 0); // Queue 0

    // CRITICAL: Add strong memory barriers after device notification
    __asm__ volatile("dmb sy" ::: "memory"); // Data memory barrier
    __asm__ volatile("dsb sy" ::: "memory"); // Data synchronization barrier
    __asm__ volatile("isb" ::: "memory");    // Instruction synchronization barrier

    tiny_printf(DEBUG, "[VIRTIO] Device notified with queue index 0\n");

    return true;
}

bool virtio_queue_wait_for_completion(void)
{
    tiny_printf(DEBUG, "[VIRTIO] Starting wait loop...\n");

    uint32_t timeout = 1000000; // Timeout counter
    uint32_t debug_counter = 0;

    while (timeout > 0)
    {
        // Invalidate used ring cache before checking for updates
        virtio_cache_invalidate_range((uint64_t)virtio_queue.used,
                                      sizeof(virtq_used_t) + virtio_queue.queue_size * sizeof(virtq_used_elem_t));

        // Safe access to used index
        volatile uint16_t used_idx = virtio_queue.used->idx;

        // Periodic debug output
        if (debug_counter % 100000 == 0)
        {
            tiny_printf(DEBUG, "[VIRTIO] Checking... used_idx=%d, last_used_idx=%d, timeout=%d\n",
                        used_idx, virtio_queue.last_used_idx, timeout);
        }

        if (used_idx != virtio_queue.last_used_idx)
        {
            tiny_printf(DEBUG, "[VIRTIO] Request completed: used_idx=%d, last_used_idx=%d\n",
                        used_idx, virtio_queue.last_used_idx);

            // Process completed requests safely
            while (virtio_queue.last_used_idx != used_idx)
            {
                uint16_t used_ring_idx = virtio_queue.last_used_idx % virtio_queue.queue_size;

                // Safe access to used ring element
                volatile virtq_used_elem_t *used_elem = &virtio_queue.used->ring[used_ring_idx];
                uint32_t elem_id = used_elem->id;
                uint32_t elem_len = used_elem->len;

                tiny_printf(DEBUG, "[VIRTIO] Completed descriptor %d, length %d\n",
                            elem_id, elem_len);

                virtio_queue.last_used_idx++;
            }

            return true;
        }

        timeout--;
        debug_counter++;
        // Small delay
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    tiny_printf(WARN, "[VIRTIO] Request timeout\n");
    return false;
}

uint64_t virtio_scan_devices(uint32_t target_device_id)
{
    tiny_printf(INFO, "[VIRTIO] Scanning for device ID %d across %d slots\n",
                target_device_id, VIRTIO_MMIO_MAX_DEVICES);

    for (uint32_t slot = 0; slot < VIRTIO_MMIO_MAX_DEVICES; slot++)
    {
        uint64_t base_addr = VIRTIO_MMIO_BASE_ADDR + (slot * VIRTIO_MMIO_DEVICE_SIZE);

        tiny_printf(DEBUG, "[VIRTIO] Checking slot %d at address 0x%x\n", slot, (uint32_t)base_addr);

        // Read magic number
        uint32_t magic = virtio_read32(base_addr + VIRTIO_MMIO_MAGIC);
        if (magic != VIRTIO_MAGIC_VALUE)
        {
            tiny_printf(DEBUG, "[VIRTIO] Slot %d: Invalid magic 0x%x (expected 0x%x)\n",
                        slot, magic, VIRTIO_MAGIC_VALUE);
            continue;
        }

        // Read version
        uint32_t version = virtio_read32(base_addr + VIRTIO_MMIO_VERSION);
        if (version != 1)
        {
            tiny_printf(DEBUG, "[VIRTIO] Slot %d: Unsupported version %d\n", slot, version);
            continue;
        }

        // Read device ID
        uint32_t device_id = virtio_read32(base_addr + VIRTIO_MMIO_DEVICE_ID);
        tiny_printf(DEBUG, "[VIRTIO] Slot %d: Found device ID %d (magic=0x%x, version=%d)\n",
                    slot, device_id, magic, version);

        if (device_id == 0)
        {
            tiny_printf(DEBUG, "[VIRTIO] Slot %d: Empty slot (device_id = 0)\n", slot);
            continue;
        }

        if (device_id == target_device_id)
        {
            tiny_printf(INFO, "[VIRTIO] Found target device ID %d at slot %d (address 0x%x)\n",
                        target_device_id, slot, (uint32_t)base_addr);
            return base_addr;
        }
        else
        {
            tiny_printf(DEBUG, "[VIRTIO] Slot %d: Device ID %d does not match target %d\n",
                        slot, device_id, target_device_id);
        }
    }

    tiny_printf(WARN, "[VIRTIO] Device ID %d not found in any of %d slots\n",
                target_device_id, VIRTIO_MMIO_MAX_DEVICES);
    return 0;
}
