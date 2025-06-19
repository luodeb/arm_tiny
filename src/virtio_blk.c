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
#include "string.h"

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

// 修复后的设置临时队列函数
static int virtio_blk_setup_temporary_queue(struct virtio_blk_device *blk_dev)
{
    struct virtio_device *vdev = blk_dev->vdev;

    // 选择队列 0
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_SEL, 0);

    // 检查队列最大大小
    uint32_t queue_num_max = virtio_read_reg32(vdev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    tiny_printf(DEBUG, "virtio_blk: queue max size = %d\n", queue_num_max);
    if (queue_num_max == 0)
    {
        tiny_printf(WARN, "virtio_blk: queue 0 not available\n");
        return VIRTIO_ERROR_NO_DEVICE;
    }

    // 设置队列大小为 4 (更安全的大小)
    uint32_t queue_size = (queue_num_max < 4) ? queue_num_max : 4;
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_NUM, queue_size);

    // 确保数据结构对齐并清零
    // memset(g_temp_desc_table, 0, sizeof(g_temp_desc_table));
    // memset(&g_temp_avail, 0, sizeof(g_temp_avail));
    // memset(&g_temp_used, 0, sizeof(g_temp_used));

    // 计算物理地址并确保对齐
    uint64_t desc_phys = (uint64_t)g_temp_desc_table;
    uint64_t avail_phys = (uint64_t)&g_temp_avail;
    uint64_t used_phys = (uint64_t)&g_temp_used;

    // 检查对齐
    if (desc_phys & 0xF)
    {
        tiny_printf(WARN, "virtio_blk: descriptor table not 16-byte aligned\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(DEBUG, "virtio_blk: setting queue addresses - desc: 0x%x, avail: 0x%x, used: 0x%x\n",
                desc_phys, avail_phys, used_phys);

    // 设置队列地址
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(desc_phys & 0xFFFFFFFF));
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(desc_phys >> 32));

    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(avail_phys & 0xFFFFFFFF));
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(avail_phys >> 32));

    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(used_phys & 0xFFFFFFFF));
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(used_phys >> 32));

    // 启用队列
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_READY, 1);

    // 确认队列就绪状态
    uint32_t ready = virtio_read_reg32(vdev, VIRTIO_MMIO_QUEUE_READY);
    if (!ready)
    {
        tiny_printf(WARN, "virtio_blk: queue failed to become ready\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    // 确保设备处于运行状态
    uint32_t status = virtio_read_reg32(vdev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
    {
        tiny_printf(WARN, "virtio_blk: device not in DRIVER_OK state (status=0x%x)\n", status);
    }

    tiny_printf(DEBUG, "virtio_blk: temporary queue setup complete\n");
    return VIRTIO_OK;
}

// 构建读取请求结构
static int virtio_blk_build_read_request(uint64_t sector, void *buffer)
{
    // 构建请求头
    g_temp_req_header.type = VIRTIO_BLK_T_IN;
    g_temp_req_header.reserved = 0;
    g_temp_req_header.sector = sector;

    // 初始化状态字节
    g_temp_status = 0xFF;

    // 构建描述符链
    // 描述符 0: 请求头 (设备读取)
    g_temp_desc_table[0].addr = (uint64_t)&g_temp_req_header;
    g_temp_desc_table[0].len = sizeof(struct virtio_blk_req_header);
    g_temp_desc_table[0].flags = VIRTQ_DESC_F_NEXT;
    g_temp_desc_table[0].next = 1;

    // 描述符 1: 数据缓冲区 (设备写入)
    g_temp_desc_table[1].addr = (uint64_t)buffer;
    g_temp_desc_table[1].len = VIRTIO_BLK_SECTOR_SIZE;
    g_temp_desc_table[1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    g_temp_desc_table[1].next = 2;

    // 描述符 2: 状态字节 (设备写入)
    g_temp_desc_table[2].addr = (uint64_t)&g_temp_status;
    g_temp_desc_table[2].len = 1;
    g_temp_desc_table[2].flags = VIRTQ_DESC_F_WRITE;
    g_temp_desc_table[2].next = 0;

    tiny_printf(DEBUG, "virtio_blk: read request built for sector %u\n", sector);
    return VIRTIO_OK;
}

// 修复后的提交请求函数
static int virtio_blk_submit_request(struct virtio_blk_device *blk_dev)
{
    struct virtio_device *vdev = blk_dev->vdev;

    // 将描述符链头添加到可用环
    uint16_t avail_idx = g_temp_avail.idx;
    g_temp_avail.ring[avail_idx & 3] = 0; // 使用位掩码而不是模数

    // 内存屏障
    __asm__ __volatile__("dmb sy" ::: "memory");

    // 更新可用环索引
    g_temp_avail.idx = avail_idx + 1;

    // 内存屏障
    __asm__ __volatile__("dmb sy" ::: "memory");

    // 通知设备
    virtio_write_reg32(vdev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    tiny_printf(DEBUG, "virtio_blk: request submitted to device (avail_idx=%d)\n", avail_idx + 1);
    return VIRTIO_OK;
}

// 修复后的轮询完成函数
static int virtio_blk_poll_completion(struct virtio_blk_device *blk_dev)
{
    struct virtio_device *vdev = blk_dev->vdev;
    uint32_t timeout = 1000000; // 增加超时时间
    uint16_t last_used_idx = g_temp_used.idx;

    tiny_printf(DEBUG, "virtio_blk: polling for completion (initial used_idx=%d)\n", last_used_idx);

    // 轮询已用环
    while (timeout > 0)
    {
        __asm__ __volatile__("dmb sy" ::: "memory");
        uint16_t used_idx = g_temp_used.idx;

        if (used_idx != last_used_idx)
        {
            // 有新的完成请求
            struct virtq_used_elem *used_elem = &g_temp_used.ring[last_used_idx & 3]; // 修复：使用正确的索引

            tiny_printf(DEBUG, "virtio_blk: request completed, id=%d, len=%d, status=%d\n",
                        used_elem->id, used_elem->len, g_temp_status);

            // 检查状态
            if (g_temp_status != VIRTIO_BLK_S_OK)
            {
                tiny_printf(WARN, "virtio_blk: device returned error status %d\n", g_temp_status);
                return VIRTIO_ERROR_IO;
            }

            return VIRTIO_OK;
        }

        // 添加小延迟以避免过度轮询
        if (timeout % 10000 == 0)
        {
            // 每10000次迭代检查一次设备状态
            uint32_t status = virtio_read_reg32(vdev, VIRTIO_MMIO_STATUS);
            if (!(status & VIRTIO_STATUS_DRIVER_OK))
            {
                tiny_printf(WARN, "virtio_blk: device status changed during operation (0x%x)\n", status);
                return VIRTIO_ERROR_IO;
            }
        }

        timeout--;
    }

    tiny_printf(WARN, "virtio_blk: request timeout (final used_idx=%d)\n", g_temp_used.idx);
    return VIRTIO_ERROR_TIMEOUT;
}

/*
 * 真实的 VirtIO 块设备读取函数
 * 使用同步队列操作与真实的 VirtIO 设备进行通信
 *
 * 参数:
 *   blk_dev: VirtIO 块设备指针
 *   sector: 要读取的扇区号
 *   buffer: 用于存储扇区数据的缓冲区 (512 字节)
 *
 * 返回:
 *   VIRTIO_OK: 成功
 *   VIRTIO_ERROR_*: 各种错误情况
 */
// 真实的 VirtIO 块设备读取函数
int virtio_blk_simple_read_sector(struct virtio_blk_device *blk_dev,
                                  uint64_t sector, void *buffer)
{
    if (!blk_dev || !buffer)
    {
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    if (!blk_dev->initialized)
    {
        tiny_printf(WARN, "virtio_blk: device not initialized\n");
        return VIRTIO_ERROR_INVALID_DEVICE;
    }

    tiny_printf(INFO, "virtio_blk: reading sector %d from real device\n", sector);

    // 设置临时队列
    int ret = virtio_blk_setup_temporary_queue(blk_dev);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: failed to setup queue\n");
        return ret;
    }

    // 构建读取请求
    ret = virtio_blk_build_read_request(sector, buffer);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: failed to build request\n");
        return ret;
    }

    // 提交请求
    ret = virtio_blk_submit_request(blk_dev);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: failed to submit request\n");
        return ret;
    }

    // 等待完成
    ret = virtio_blk_poll_completion(blk_dev);
    if (ret != VIRTIO_OK)
    {
        tiny_printf(WARN, "virtio_blk: request failed or timeout\n");
        return ret;
    }

    tiny_printf(DEBUG, "virtio_blk: successfully read sector %llu from real device\n", sector);
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
