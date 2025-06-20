/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"
#include "virtio/virtio_blk.h"
#include "virtio/fat32.h"
#include "virtio/virtio_debug.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

int kernel_main(void)
{
    tiny_printf(INFO, "\nHello, ARM Tiny VM%s!\n", VM_VERSION);
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

    // Test 7: File Reading Test
    tiny_printf(INFO, "=== Testing File Reading ===\n");
    char file_content[256];
    if (!fat32_read_file("hello.txt", file_content, 255))
    {
        tiny_printf(WARN, "Failed to read hello.txt file\n");
    }
    else
    {
        file_content[255] = '\0'; // Ensure null termination
        tiny_printf(INFO, "Successfully read hello.txt file\n");
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
