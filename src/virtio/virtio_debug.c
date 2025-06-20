/*
 * virtio_debug.c
 *
 * Minimal debug version to isolate the hang issue
 */

#include "virtio/virtio_mmio.h"
#include "tiny_io.h"
#include <stddef.h>

// Test basic memory and register access
bool virtio_test_basic_access(void)
{
    tiny_printf(DEBUG, "[VIRTIO] Testing basic access...\n");
    
    virtio_device_t *dev = virtio_get_device();
    if (dev == NULL) {
        tiny_printf(WARN, "[VIRTIO] Device not initialized\n");
        return false;
    }
    
    // Test device register reads
    uint32_t magic = virtio_read32(dev->base_addr + VIRTIO_MMIO_MAGIC);
    tiny_printf(DEBUG, "[VIRTIO] Magic value: 0x%x\n", magic);
    
    uint32_t version = virtio_read32(dev->base_addr + VIRTIO_MMIO_VERSION);
    tiny_printf(DEBUG, "[VIRTIO] Version: 0x%x\n", version);
    
    uint32_t device_id = virtio_read32(dev->base_addr + VIRTIO_MMIO_DEVICE_ID);
    tiny_printf(DEBUG, "[VIRTIO] Device ID: 0x%x\n", device_id);
    
    // Test queue memory access
    virtqueue_t *queue = virtio_get_queue();
    if (queue != NULL && queue->desc != NULL) {
        tiny_printf(DEBUG, "[VIRTIO] Queue desc addr: 0x%p\n", queue->desc);
        tiny_printf(DEBUG, "[VIRTIO] First descriptor addr: 0x%x\n", (uint32_t)queue->desc[0].addr);
    }
    
    if (queue != NULL && queue->avail != NULL) {
        tiny_printf(DEBUG, "[VIRTIO] Queue avail addr: 0x%p\n", queue->avail);
        tiny_printf(DEBUG, "[VIRTIO] Avail idx: %d\n", queue->avail->idx);
    }
    
    if (queue != NULL && queue->used != NULL) {
        tiny_printf(DEBUG, "[VIRTIO] Queue used addr: 0x%p\n", queue->used);
        tiny_printf(DEBUG, "[VIRTIO] Used idx: %d\n", queue->used->idx);
    }
    
    tiny_printf(DEBUG, "[VIRTIO] Basic access test completed\n");
    return true;
}

// Test hang point isolation
bool virtio_test_hang_points(void)
{
    tiny_printf(DEBUG, "[VIRTIO] Testing potential hang points...\n");
    
    // Test 1: Simple counter loop
    tiny_printf(DEBUG, "[VIRTIO] Test 1: Simple counter loop\n");
    for (int i = 0; i < 1000; i++) {
        if (i % 100 == 0) {
            tiny_printf(DEBUG, "[VIRTIO] Counter: %d\n", i);
        }
    }
    
    // Test 2: Memory barrier operations
    tiny_printf(DEBUG, "[VIRTIO] Test 2: Memory barriers\n");
    __asm__ volatile("dmb sy" ::: "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    
    // Test 3: Simple register access in loop (only if device is available)
    tiny_printf(DEBUG, "[VIRTIO] Test 3: Register access loop\n");
    virtio_device_t *dev = virtio_get_device();
    if (dev != NULL && dev->ready) {
        for (int i = 0; i < 100; i++) {
            uint32_t status = virtio_read32(dev->base_addr + VIRTIO_MMIO_STATUS);
            if (i % 20 == 0) {
                tiny_printf(DEBUG, "[VIRTIO] Status: 0x%x\n", status);
            }
        }
    } else {
        tiny_printf(DEBUG, "[VIRTIO] Device not ready, skipping register access\n");
    }
    
    // Test 4: Queue memory access without cache ops
    tiny_printf(DEBUG, "[VIRTIO] Test 4: Queue memory access\n");
    virtqueue_t *queue = virtio_get_queue();
    if (queue != NULL && queue->used != NULL) {
        for (int i = 0; i < 10; i++) {
            volatile uint16_t used_idx = queue->used->idx;
            tiny_printf(DEBUG, "[VIRTIO] Used idx read %d: %d\n", i, used_idx);
        }
    } else {
        tiny_printf(DEBUG, "[VIRTIO] Queue not ready, skipping memory access\n");
    }
    
    tiny_printf(DEBUG, "[VIRTIO] Hang point tests completed\n");
    return true;
}
