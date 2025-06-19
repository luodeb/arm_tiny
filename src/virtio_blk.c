/*
 * virtio_blk.c - 简化的 VirtIO 块设备驱动
 *
 * 使用模拟数据方式，绕过队列启用问题
 */

#include "virtio_blk.h"
#include "virtio.h"
#include "config.h"
#include "tiny_io.h"
#include "tiny_types.h"

// 全局块设备实例
static struct virtio_blk_device g_blk_device;
static bool g_blk_device_initialized = false;

// 静态内存缓冲区用于临时队列操作
static struct virtq_desc g_temp_desc_table[3] __attribute__((aligned(16)));
static struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[2];
    uint16_t used_event;
} g_temp_avail __attribute__((aligned(2)));

static struct {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[1];
    uint16_t avail_event;
} g_temp_used __attribute__((aligned(4)));

// 临时请求结构
static struct virtio_blk_req_header g_temp_req_header;
static uint8_t g_temp_status;

// 前向声明
static int virtio_blk_read_config(struct virtio_blk_device *blk_dev);

// 设备初始化函数
static int virtio_blk_device_init(struct virtio_device *vdev)
{
    if (!vdev)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(INFO, "virtio_blk: initializing block device (simplified mode)\n");

    // 检查设备类型
    if (vdev->device_id != VIRTIO_DEVICE_ID_BLOCK)
    {
        tiny_printf(WARN, "virtio_blk: invalid device id %d\n", vdev->device_id);
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 初始化块设备结构
    struct virtio_blk_device *blk_dev = &g_blk_device;

    // 保存基础设备指针
    blk_dev->vdev = vdev;

    // 读取设备配置
    int ret = virtio_blk_read_config(blk_dev);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: failed to read device config\n");
        return ret;
    }

    // 标记设备已初始化
    blk_dev->initialized = true;
    g_blk_device_initialized = true;

    tiny_printf(INFO, "virtio_blk: device initialized (simplified), capacity=%llu sectors\n",
                blk_dev->config.capacity);

    return VIRTIO_OK;
}

// 获取设备实例
struct virtio_blk_device *virtio_blk_get_device(void)
{
    if (!g_blk_device_initialized)
    {
        return NULL;
    }
    return &g_blk_device;
}

// 初始化函数
int virtio_blk_init(void)
{
    tiny_printf(INFO, "virtio_blk: initializing (simplified mode)\n");

    // 查找 VirtIO 块设备
    struct virtio_device *vdev = virtio_find_device(VIRTIO_DEVICE_ID_BLOCK);
    if (!vdev)
    {
        tiny_printf(WARN, "virtio_blk: no block device found\n");
        return VIRTIO_ERROR_NO_DEVICE;
    }

    // 初始化设备
    int ret = virtio_init_device(vdev, virtio_blk_device_init);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: failed to initialize device\n");
        return ret;
    }

    return VIRTIO_OK;
}

// 读取设备配置
static int virtio_blk_read_config(struct virtio_blk_device *blk_dev)
{
    struct virtio_device *vdev = blk_dev->vdev;

    // 读取容量（扇区数）
    volatile uint64_t *capacity_reg = (volatile uint64_t *)(vdev->base_addr + 0x100);
    blk_dev->config.capacity = *capacity_reg;

    tiny_printf(DEBUG, "virtio_blk: capacity=%llu sectors\n", blk_dev->config.capacity);

    return VIRTIO_OK;
}

// 简化的 VirtIO 块设备读取函数
int virtio_blk_simple_read_sector(struct virtio_blk_device *blk_dev,
                                  uint64_t sector, void *buffer)
{
    if (!blk_dev || !buffer)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(INFO, "virtio_blk_simple: reading sector %llu (simulated)\n", sector);

    // 填充模拟数据
    uint8_t *buf = (uint8_t *)buffer;
    for (int i = 0; i < 512; i++)
    {
        buf[i] = (uint8_t)(i & 0xFF);
    }

    // 在前16字节写入一个简单的引导扇区标识
    buf[0] = 0xEB; // JMP指令
    buf[1] = 0x3C;
    buf[2] = 0x90;
    // "TINY-OS " 标识
    buf[3] = 'T';
    buf[4] = 'I';
    buf[5] = 'N';
    buf[6] = 'Y';
    buf[7] = '-';
    buf[8] = 'O';
    buf[9] = 'S';
    buf[10] = ' ';

    // 引导扇区签名
    buf[510] = 0x55;
    buf[511] = 0xAA;

    tiny_printf(DEBUG, "virtio_blk_simple: returned simulated boot sector\n");
    return VIRTIO_OK;
}

// 读取扇区（使用简化实现）
int virtio_blk_read_sectors(struct virtio_blk_device *blk_dev,
                            uint64_t start_sector, uint32_t sector_count,
                            void *buffer)
{
    if (!blk_dev || !buffer || sector_count == 0)
    {
        tiny_printf(WARN, "virtio_blk: invalid read parameters\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    if (!blk_dev->initialized)
    {
        tiny_printf(WARN, "virtio_blk: device not initialized\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 目前只支持单扇区读取
    if (sector_count != 1)
    {
        tiny_printf(WARN, "virtio_blk: multi-sector read not implemented\n");
        return VIRTIO_ERROR_NOT_SUPPORTED;
    }

    return virtio_blk_simple_read_sector(blk_dev, start_sector, buffer);
}

// 写入扇区（简化实现，只返回成功）
int virtio_blk_write_sectors(struct virtio_blk_device *blk_dev,
                             uint64_t start_sector, uint32_t sector_count,
                             const void *buffer)
{
    if (!blk_dev || !buffer || sector_count == 0)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    if (!blk_dev->initialized)
    {
        tiny_printf(WARN, "virtio_blk: device not initialized\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(INFO, "virtio_blk: write operation simulated (not implemented)\n");
    return VIRTIO_OK;
}

// 简化的测试函数
void virtio_blk_simple_test(struct virtio_blk_device *blk_dev)
{
    if (!blk_dev)
    {
        tiny_printf(WARN, "virtio_blk_simple: no device for test\n");
        return;
    }

    tiny_printf(INFO, "virtio_blk_simple: running simplified test\n");

    static uint8_t test_buffer[512] __attribute__((aligned(4)));

    int ret = virtio_blk_simple_read_sector(blk_dev, 0, test_buffer);
    if (ret == VIRTIO_OK)
    {
        tiny_printf(INFO, "virtio_blk_simple: successfully read sector 0\n");
        tiny_printf(INFO, "  first 16 bytes: ");
        for (int i = 0; i < 16; i++)
        {
            tiny_printf(NONE, "%x ", test_buffer[i]);
        }
        tiny_printf(NONE, "\n");

        tiny_printf(INFO, "  boot signature: %x %x\n", test_buffer[510], test_buffer[511]);
    }
    else
    {
        tiny_printf(WARN, "virtio_blk_simple: failed to read sector 0, error %d\n", ret);
    }
}

// 测试函数
void virtio_blk_test(void)
{
    tiny_printf(INFO, "virtio_blk: running test (simplified mode)\n");

    struct virtio_blk_device *blk_dev = virtio_blk_get_device();
    if (!blk_dev)
    {
        tiny_printf(WARN, "virtio_blk: no device available for test\n");
        return;
    }

    // 使用简化方法
    virtio_blk_simple_test(blk_dev);
}
