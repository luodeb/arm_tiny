/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tinyio.h"
#include "tinystd.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

// Forward declaration
int virtio_block_test(void);

int kernel_main(void)
{
    tiny_info("Hello, ARM Tiny VM [%s]!\n", VM_VERSION);

    // Test VirtIO block device
    if (virtio_block_test() < 0)
    {
        tiny_error("VirtIO block test failed\n");
    }

    system_shutdown();
    return 0;
}
