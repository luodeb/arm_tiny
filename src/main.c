/*
 * File: main.c
 * Author: Debin
 * Date: 2024-11-15
 * Description: Main file for the ARM Tiny project.
 */

#include "tiny_io.h"
#include "handle.h"
#include "gicv2.h"
#include "virtio.h"
#include "virtio_blk.h"
#include "simple_fs.h"

#ifndef VM_VERSION
#define VM_VERSION "null"
#endif

int timer_interval = 60000000;
volatile int flag = 123;

void timer_gic_init(void)
{
    write32(GICD_CTRL_ENABLE_GROUP0 | GICD_CTRL_ENABLE_GROUP1, (void *)GICD_CTLR);

    // 允许所有优先级的中断
    write32(0xff - 7, (void *)GICC_PMR);
    write32(GICC_CTRL_ENABLE | (1 << 9), (void *)GICC_CTLR);

    uint32_t value;

    value = read32((void *)(uint32_t)GICD_ISENABLER(0));
    value |= (1 << 30);
    write32(value, (void *)(uint32_t)GICD_ISENABLER(0));
}

static int count = 0;
void timer_handler(uint64_t *)
{
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(timer_interval));
    tiny_printf(INFO, "irq %d, count %d\n\n", TIMER, count++);
}

int kernel_main(void)
{
    tiny_printf(INFO, "\nHello, ARM Tiny VM%s with Virtio Support!\n", VM_VERSION);

    // 初始化 I/O 系统
    tiny_io_init();

    // 初始化中断处理
    handle_init();

    tiny_printf(INFO, "Basic system initialization complete\n");

    // 初始化 virtio 子系统
    tiny_printf(INFO, "Initializing Virtio subsystem...\n");
    int ret = virtio_init();
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "Failed to initialize Virtio subsystem: %d\n", ret);
        goto error_loop;
    }

    // 运行 virtio 基本测试
    virtio_test();

    // 初始化 virtio-blk 设备
    tiny_printf(INFO, "Initializing Virtio block device...\n");
    ret = virtio_blk_init();
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "Failed to initialize Virtio block device: %d\n", ret);
        goto error_loop;
    }

    // 运行 virtio-blk 测试
    virtio_blk_test();

    // 初始化文件系统
    struct virtio_blk_device *blk_dev = virtio_blk_get_device();
    if (blk_dev)
    {
        tiny_printf(INFO, "Initializing file system...\n");
        ret = fs_init(blk_dev);
        if (ret == FS_OK)
        {
            ret = fs_mount();
            if (ret == FS_OK)
            {
                tiny_printf(INFO, "File system mounted successfully!\n");
                fs_test_basic_operations();
                fs_test_file_reading();
            }
            else
            {
                tiny_printf(ERROR, "Failed to mount file system: %d\n", ret);
            }
        }
        else
        {
            tiny_printf(ERROR, "Failed to initialize file system: %d\n", ret);
        }
    }

    tiny_printf(INFO, "Virtio driver initialization complete!\n");
    tiny_printf(INFO, "System ready. Entering idle loop...\n");

error_loop:
    // 进入空闲循环
    while (1)
    {
    }

    return 0;
}
