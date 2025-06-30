/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"
#include "gicv2.h"
#include "timer.h"
#include "virtio/virtio_blk.h"
#include "virtio/fat32.h"
#include "virtio/virtio_debug.h"
#include "virtio/virtio_interrupt.h"
#include "config.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

// Simple strlen implementation for bare metal environment
static uint32_t my_strlen(const char *str)
{
    uint32_t len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

int kernel_main(void)
{
    tiny_io_init();
    tiny_printf(INFO, "\nHello, ARM Tiny VM%s!\n", VM_VERSION);

#if USE_VIRTIO_IRQ
    // Initialize GIC
    tiny_printf(INFO, "=== Initializing GIC ===\n");
    gic_init();

    // Initialize Timer
    tiny_printf(INFO, "=== Initializing Timer ===\n");
    timer_init();

    // Test Timer interrupts
    tiny_printf(INFO, "=== Testing Timer Interrupts ===\n");
    if (!timer_test_simple())
    {
        tiny_printf(WARN, "Timer interrupt test FAILED\n");
        goto error_exit;
    }
    tiny_printf(INFO, "Timer interrupt test PASSED\n");
    // Initialize VirtIO Interrupts
    tiny_printf(INFO, "=== Initializing VirtIO Interrupts ===\n");
    if (!virtio_interrupt_init())
    {
        tiny_printf(WARN, "VirtIO interrupt initialization FAILED\n");
        goto error_exit;
    }
    tiny_printf(INFO, "VirtIO interrupt initialization SUCCESSFUL\n");
#else
    tiny_printf(INFO, "VirtIO interrupts are disabled (USE_VIRTIO_IRQ=0)\n");
#endif

    tiny_printf(INFO, "Starting VirtIO Debug Tests...\n");
    // Test 1: Basic hang point isolation
    tiny_printf(INFO, "=== Testing Hang Points ===\n");
    if (!virtio_test_hang_points())
    {
        tiny_printf(WARN, "Hang point tests FAILED\n");
        goto error_exit;
    }

    // Test 2: Initialize VirtIO and test basic access
    tiny_printf(INFO, "=== Testing VirtIO Initialization ===\n");
    if (!virtio_blk_init())
    {
        tiny_printf(WARN, "VirtIO Block device initialization FAILED\n");
        goto error_exit;
    }

    // Test 3: Basic device access
    tiny_printf(INFO, "=== Testing Basic Device Access ===\n");
    if (!virtio_test_basic_access())
    {
        tiny_printf(WARN, "Basic access test FAILED\n");
        goto error_exit;
    }

    // Test 5: VirtIO Block Sector Read Test
    tiny_printf(INFO, "=== Testing VirtIO Block Sector Read ===\n");
    uint8_t sector_buffer[512];
    if (!virtio_blk_read_sector(0, sector_buffer))
    {
        tiny_printf(ERROR, "Failed to read sector 0 (boot sector)\n");
        goto error_exit;
    }

    tiny_printf(INFO, "Successfully read boot sector (sector 0)\n");
    tiny_printf(DEBUG, "Boot sector first 64 bytes:\n");
    for (int i = 0; i < 64; i++)
    {
        if (i % 16 == 0)
            tiny_printf(DEBUG, "%04x: ", i);
        tiny_printf(DEBUG, "%02x ", sector_buffer[i]);
        if (i % 16 == 15)
            tiny_printf(DEBUG, "\n");
    }

    // Test 6: FAT32 File System Test
    tiny_printf(INFO, "=== Testing FAT32 File System ===\n");
    if (!fat32_init())
    {
        tiny_printf(ERROR, "Failed to initialize FAT32 file system\n");
        goto error_exit;
    }
    tiny_printf(INFO, "FAT32 file system initialized successfully\n");

    // Test 7: File Writing Test
    // tiny_printf(INFO, "=== Testing File Writing ===\n");
    // const char *test_data = "Hello, this is a test file created by the FAT32 implementation!\nThis file contains multiple lines.\nLine 3 of the test file.";
    // if (!fat32_write_file("test.txt", test_data, my_strlen(test_data)))
    // {
    //     tiny_printf(WARN, "Failed to write test.txt file\n");
    // }
    // else
    // {
    //     tiny_printf(INFO, "Successfully wrote test.txt file\n");
    // }

    // Test 8: File Reading Test
    tiny_printf(INFO, "=== Testing File Reading ===\n");
    char file_content[512];
    if (!fat32_read_file("test.txt", file_content, 511))
    {
        tiny_printf(WARN, "Failed to read test.txt file\n");
    }
    else
    {
        file_content[511] = '\0'; // Ensure null termination
        tiny_printf(INFO, "Successfully read test.txt file\n");
        tiny_printf(INFO, "File content: %s\n", file_content);
    }

    tiny_printf(INFO, "All VirtIO tests completed successfully!\n");

    // Shutdown the system automatically
    system_shutdown();

    return 0;

error_exit:
    tiny_printf(WARN, "System stopped due to error\n");

    // Shutdown the system even on error
    system_shutdown();

    return -1;
}
