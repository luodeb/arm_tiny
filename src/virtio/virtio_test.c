#include "virtio/virtio_block.h"
#include "tinystd.h"
#include "tinyio.h"
#include <virtio/virtio_net.h>

// VirtIO device scanning parameters
#define VIRTIO_SCAN_BASE_ADDR 0xa000000 // Start scanning from 0xa00_0000
#define VIRTIO_SCAN_STEP 0x200          // Step size 0x200
#define VIRTIO_SCAN_COUNT 32            // Scan 32 positions

static void print_hex_dump(const void *data, uint32_t size, const char *prefix)
{
    const uint8_t *bytes = (const uint8_t *)data;

    tiny_info("%s (size: %u bytes):\n", prefix, size);

    for (uint32_t i = 0; i < size; i += 16)
    {
        // Print offset
        printf("%08x: ", i);

        // Print hex bytes
        for (uint32_t j = 0; j < 16; j++)
        {
            if (i + j < size)
            {
                printf("%02x ", bytes[i + j]);
            }
            else
            {
                printf("   ");
            }

            // Add extra space after 8 bytes
            if (j == 7)
            {
                printf(" ");
            }
        }

        // Print ASCII representation
        printf(" |");
        for (uint32_t j = 0; j < 16 && i + j < size; j++)
        {
            uint8_t c = bytes[i + j];
            if (c >= 32 && c <= 126)
            {
                printf("%c", c);
            }
            else
            {
                printf(".");
            }
        }
        printf("|\n");
    }
    printf("\n");
}

// Scan for VirtIO devices and return the address of the first block device found
static uint64_t scan_for_virtio_block_device(uint32_t found_device_id)
{
    tiny_info("Scanning for VirtIO %d devices...\n", found_device_id);

    for (uint32_t i = 0; i < VIRTIO_SCAN_COUNT; i++)
    {
        uint64_t addr = VIRTIO_SCAN_BASE_ADDR + (i * VIRTIO_SCAN_STEP);

        // Check magic value first
        uint32_t magic = read32((volatile void *)(addr + VIRTIO_MMIO_MAGIC_VALUE));
        if (magic != VIRTIO_MMIO_MAGIC)
        {
            tiny_debug("Address 0x%lx: Invalid magic 0x%x\n", addr, magic);
            continue;
        }

        // Check version
        uint32_t version = read32((volatile void *)(addr + VIRTIO_MMIO_VERSION));
        if (version < 1 || version > 2)
        {
            tiny_debug("Address 0x%lx: Invalid version %u\n", addr, version);
            continue;
        }

        // Check device ID
        uint32_t device_id = read32((volatile void *)(addr + VIRTIO_MMIO_DEVICE_ID));
        uint32_t vendor_id = read32((volatile void *)(addr + VIRTIO_MMIO_VENDOR_ID));

        tiny_info("Found VirtIO device at 0x%lx: ID=%u, Vendor=0x%x, Version=%u\n",
                  addr, device_id, vendor_id, version);

        if (device_id == found_device_id)
        {
            tiny_info("Found VirtIO %d device at address 0x%lx!\n", found_device_id, addr);
            return addr;
        }
    }

    tiny_error("No VirtIO %d device found in scan range\n", found_device_id);
    return 0;
}

int virtio_block_test(void)
{
    tiny_info("Starting VirtIO Block device test\n");

    // Scan for VirtIO block device
    uint64_t block_device_addr = scan_for_virtio_block_device(VIRTIO_ID_BLOCK);
    if (block_device_addr == 0)
    {
        tiny_error("Failed to find VirtIO block device\n");
        return -1;
    }

    // Initialize block device
    virtio_blk_device_t blk_dev;

    if (virtio_blk_init(&blk_dev, block_device_addr, 0) < 0) // Device index 0
    {
        tiny_error("Failed to initialize VirtIO block device\n");
        return -1;
    }

    // Allocate buffer for reading
    uint8_t *read_buffer = (uint8_t *)virtio_alloc(512, 16); // Allocate 512 bytes, 16-byte aligned
    if (!read_buffer)
    {
        tiny_error("Failed to allocate read buffer\n");
        return -1;
    }

    // Clear buffer
    for (int i = 0; i < 512; i++)
    {
        read_buffer[i] = 0;
    }

    tiny_info("Reading first sector (sector 0)...\n");

    // Read the first sector
    if (virtio_blk_read_sector(&blk_dev, 0, read_buffer, 1) < 0)
    {
        tiny_error("Failed to read first sector\n");
        return -1;
    }

    tiny_info("Successfully read first sector!\n");

    // Print first 64 bytes
    print_hex_dump(read_buffer, 64, "First 64 bytes of sector 0");

    // Also print some additional info about the sector
    tiny_info("Full sector size: %u bytes\n", blk_dev.block_size);
    tiny_info("Device capacity: %llu sectors\n", blk_dev.capacity);

    // Print memory allocator information
    virtio_allocator_info();

    return 0;
}

int virtio_net_basic_test(void)
{
    tiny_info("Starting VirtIO Net basic test\n");

    // Scan for VirtIO net device
    uint64_t net_device_addr = scan_for_virtio_block_device(VIRTIO_ID_NET);
    if (net_device_addr == 0)
    {
        tiny_error("Failed to find VirtIO Net device\n");
        return -1;
    }

    // Initialize net device
    virtio_net_device_t net_dev;

    if (virtio_net_init(&net_dev, net_device_addr, 1) < 0)
    { // Device index 1
        tiny_error("Failed to initialize VirtIO Net device\n");
        return -1;
    }

    // Print device information
    tiny_info("VirtIO Net device initialized successfully\n");
    virtio_net_print_mac(net_dev.mac_addr);
    tiny_info("Link status: %s\n", virtio_net_is_link_up(&net_dev) ? "UP" : "DOWN");
    tiny_info("MTU: %d\n", net_dev.mtu);

    // Print initial statistics
    virtio_net_print_stats(&net_dev);

    tiny_info("VirtIO Net basic test completed successfully\n");
    return 0;
}

void virtio_test_all(void)
{
    // Test VirtIO block device
    if (virtio_block_test() < 0)
    {
        tiny_error("VirtIO block test failed\n");
    }

    // Test VirtIO network device
    tiny_info("\n=== VirtIO Network Tests ===\n");

    if (virtio_net_basic_test() < 0)
    {
        tiny_error("VirtIO Net basic test failed\n");
    }

    // if (virtio_net_send_test() < 0)
    // {
    //     tiny_error("VirtIO Net send test failed\n");
    // }

    // if (virtio_net_receive_test() < 0)
    // {
    //     tiny_error("VirtIO Net receive test failed\n");
    // }

    // if (virtio_net_loopback_test() < 0)
    // {
    //     tiny_error("VirtIO Net loopback test failed\n");
    // }

    // if (virtio_net_layered_send_test() < 0)
    // {
    //     tiny_error("VirtIO Net layered send test failed\n");
    // }
}
