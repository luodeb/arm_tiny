/*
 * virtio.c - 简化的 VirtIO 设备驱动核心实现
 */

#include "virtio.h"
#include "config.h"
#include "tiny_io.h"
#include "tiny_types.h"

// 全局设备数组
static struct virtio_device g_virtio_devices[VIRTIO_MAX_DEVICES];
static uint32_t g_num_devices = 0;

// MMIO 寄存器访问函数
static uint32_t virtio_mmio_read32(struct virtio_device *vdev, uint32_t offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)(vdev->base_addr + offset);
    return *reg;
}

static void virtio_mmio_write32(struct virtio_device *vdev, uint32_t offset, uint32_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(vdev->base_addr + offset);
    *reg = value;
}

// 公共寄存器访问函数
uint32_t virtio_read_reg32(struct virtio_device *vdev, uint32_t offset)
{
    return virtio_mmio_read32(vdev, offset);
}

void virtio_write_reg32(struct virtio_device *vdev, uint32_t offset, uint32_t value)
{
    virtio_mmio_write32(vdev, offset, value);
}

// 设备探测函数
static int virtio_probe_device(uint32_t base_addr)
{
    volatile uint32_t *reg = (volatile uint32_t *)base_addr;

    // 检查魔数
    uint32_t magic = reg[VIRTIO_MMIO_MAGIC / 4];
    if (magic != VIRTIO_MMIO_MAGIC_VALUE)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 检查版本
    uint32_t version = reg[VIRTIO_MMIO_VERSION / 4];
    if (version != 1)
    {
        tiny_printf(WARN, "virtio: unsupported version %d at 0x%x\n", version, base_addr);
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 读取设备ID
    uint32_t device_id = reg[VIRTIO_MMIO_DEVICE_ID / 4];
    if (device_id == 0)
    {
        // 没有设备
        return VIRTIO_ERROR_NO_DEVICE;
    }

    // 读取厂商ID
    uint32_t vendor_id = reg[VIRTIO_MMIO_VENDOR_ID / 4];

    tiny_printf(DEBUG, "virtio: found device id=%d vendor=0x%x at 0x%x\n",
                device_id, vendor_id, base_addr);

    // 添加到设备列表
    if (g_num_devices >= VIRTIO_MAX_DEVICES)
    {
        tiny_printf(WARN, "virtio: too many devices, ignoring device at 0x%x\n", base_addr);
        return VIRTIO_ERROR_NO_MEMORY;
    }

    struct virtio_device *vdev = &g_virtio_devices[g_num_devices];
    vdev->base_addr = base_addr;
    vdev->device_id = device_id;
    vdev->vendor_id = vendor_id;
    vdev->status = 0;

    g_num_devices++;

    return VIRTIO_OK;
}

// 设备初始化
int virtio_init_device(struct virtio_device *vdev, virtio_device_init_func init_func)
{
    if (!vdev || !init_func)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(INFO, "virtio: initializing device id=%d\n", vdev->device_id);

    // 重置设备
    vdev->status = 0;
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, 0);

    // 设置 ACKNOWLEDGE 状态
    vdev->status |= VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);

    // 设置 DRIVER 状态
    vdev->status |= VIRTIO_STATUS_DRIVER;
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);

    // 读取设备特性
    uint32_t device_features = virtio_mmio_read32(vdev, VIRTIO_MMIO_DEVICE_FEATURES);
    uint32_t driver_features = device_features; // 简化：接受所有特性

    tiny_printf(DEBUG, "virtio: device features: 0x%x, driver features: 0x%x\n",
                device_features, driver_features);

    // 写入驱动特性
    virtio_mmio_write32(vdev, VIRTIO_MMIO_DRIVER_FEATURES, driver_features);

    // 设置 FEATURES_OK 状态
    vdev->status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);

    // 验证 FEATURES_OK
    uint32_t status = virtio_mmio_read32(vdev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK))
    {
        tiny_printf(WARN, "virtio: device rejected features\n");
        vdev->status |= VIRTIO_STATUS_FAILED;
        virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 调用设备特定的初始化函数
    int ret = init_func(vdev);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio: device init failed\n");
        vdev->status |= VIRTIO_STATUS_FAILED;
        virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);
        return ret;
    }

    // 设置 DRIVER_OK 状态
    vdev->status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, vdev->status);

    tiny_printf(INFO, "virtio: device id=%d initialized successfully\n", vdev->device_id);
    return VIRTIO_OK;
}

// 查找设备
struct virtio_device *virtio_find_device(uint32_t device_id)
{
    for (uint32_t i = 0; i < g_num_devices; i++)
    {
        if (g_virtio_devices[i].device_id == device_id)
        {
            return &g_virtio_devices[i];
        }
    }
    return NULL;
}

// 初始化 VirtIO 子系统
int virtio_init(void)
{
    tiny_printf(INFO, "virtio: initializing subsystem\n");

    g_num_devices = 0;

    // 探测设备
    for (uint32_t addr = VIRTIO_MMIO_BASE; addr < VIRTIO_MMIO_BASE + VIRTIO_MMIO_SIZE; addr += VIRTIO_MMIO_STRIDE)
    {
        tiny_printf(DEBUG, "virtio: probing device at 0x%x: magic=0x%x (expected 0x%x)\n",
                    addr, *(volatile uint32_t *)addr, VIRTIO_MMIO_MAGIC_VALUE);

        if (*(volatile uint32_t *)addr == VIRTIO_MMIO_MAGIC_VALUE)
        {
            uint32_t device_id = *(volatile uint32_t *)(addr + VIRTIO_MMIO_DEVICE_ID);
            tiny_printf(DEBUG, "virtio: device_id at 0x%x = %d\n", addr, device_id);

            if (device_id != 0)
            {
                virtio_probe_device(addr);
            }
        }
    }

    tiny_printf(INFO, "virtio: found %d devices\n", g_num_devices);
    return VIRTIO_OK;
}

// 测试函数
void virtio_test(void)
{
    tiny_printf(INFO, "virtio: running basic test\n");

    // 列出所有发现的设备
    for (uint32_t i = 0; i < g_num_devices; i++)
    {
        struct virtio_device *vdev = &g_virtio_devices[i];
        tiny_printf(INFO, "  device %d: id=%d vendor=%d base=0x%x\n",
                    i, vdev->device_id, vdev->vendor_id, vdev->base_addr);
    }
}