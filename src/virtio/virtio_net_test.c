#include "virtio/virtio_net.h"
#include "tinystd.h"
#include "tinyio.h"

// Test constants
#define VIRTIO_SCAN_BASE_ADDR 0x0a000000
#define VIRTIO_SCAN_STEP 0x200
#define VIRTIO_SCAN_COUNT 32

// Simple Ethernet frame for testing
static const uint8_t test_ethernet_frame[] = {
    // Destination MAC (broadcast)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // Source MAC (our test MAC)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    // EtherType (IPv4)
    0x08, 0x00,
    // Simple IPv4 packet (ping request)
    0x45, 0x00, 0x00, 0x1C, // Version, IHL, ToS, Total Length
    0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment Offset
    0x40, 0x01, 0x00, 0x00, // TTL, Protocol (ICMP), Checksum
    0xC0, 0xA8, 0x01, 0x01, // Source IP (192.168.1.1)
    0xC0, 0xA8, 0x01, 0x02, // Dest IP (192.168.1.2)
    // ICMP header
    0x08, 0x00, 0x00, 0x00, // Type (Echo Request), Code, Checksum, ID
    0x48, 0x65, 0x6C, 0x6C  // Data: "Hell"
};

// Scan for VirtIO devices and return the address of the first net device found
static uint64_t scan_for_virtio_net_device(void)
{
    tiny_info("Scanning for VirtIO Net devices...\n");

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
        if (device_id == VIRTIO_ID_NET)
        {
            tiny_info("Found VirtIO Net device at 0x%lx (version %u)\n", addr, version);
            return addr;
        }
        else
        {
            tiny_debug("Address 0x%lx: Different device ID %u\n", addr, device_id);
        }
    }

    return 0;
}

int virtio_net_send_test(void)
{
    tiny_info("Starting VirtIO Net send test\n");

    // Scan for VirtIO net device
    uint64_t net_device_addr = scan_for_virtio_net_device();
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

    // Check link status
    if (!virtio_net_is_link_up(&net_dev))
    {
        tiny_warn("Network link is down, but continuing test\n");
    }

    // Send test packet
    tiny_info("Sending test Ethernet frame (%d bytes)\n", sizeof(test_ethernet_frame));

    int result = virtio_net_send_packet(&net_dev, test_ethernet_frame, sizeof(test_ethernet_frame));
    if (result < 0)
    {
        tiny_error("Failed to send test packet\n");
        return -1;
    }

    tiny_info("Test packet sent successfully\n");

    // Print statistics
    virtio_net_print_stats(&net_dev);

    tiny_info("VirtIO Net send test completed successfully\n");
    return 0;
}

int virtio_net_receive_test(void)
{
    tiny_info("Starting VirtIO Net receive test\n");

    // Scan for VirtIO net device
    uint64_t net_device_addr = scan_for_virtio_net_device();
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

    // Check link status
    if (!virtio_net_is_link_up(&net_dev))
    {
        tiny_warn("Network link is down, but continuing test\n");
    }

    // Allocate receive buffer
    uint8_t rx_buffer[1600]; // Large enough for most Ethernet frames
    uint32_t received_len;

    tiny_info("Waiting for incoming packets (polling for 10 seconds)...\n");

    // Poll for incoming packets
    int packets_received = 0;
    for (int i = 0; i < 1000; i++)
    { // 10 seconds with 10ms intervals
        int result = virtio_net_receive_packet(&net_dev, rx_buffer, sizeof(rx_buffer), &received_len);
        if (result == 0)
        {
            packets_received++;
            tiny_info("Received packet %d: %d bytes\n", packets_received, received_len);

            // Print first few bytes of the packet
            tiny_info("  Data: ");
            for (int j = 0; j < (received_len < 16 ? received_len : 16); j++)
            {
                tiny_info("%02x ", rx_buffer[j]);
            }
            tiny_info("\n");

            if (packets_received >= 5)
            {
                tiny_info("Received enough packets for test\n");
                break;
            }
        }

        // Small delay (approximately 10ms)
        for (volatile int j = 0; j < 100000; j++)
            ;
    }

    if (packets_received == 0)
    {
        tiny_warn("No packets received during test period\n");
    }
    else
    {
        tiny_info("Received %d packets total\n", packets_received);
    }

    // Print statistics
    virtio_net_print_stats(&net_dev);

    tiny_info("VirtIO Net receive test completed\n");
    return 0;
}

int virtio_net_loopback_test(void)
{
    tiny_info("Starting VirtIO Net loopback test\n");

    // Scan for VirtIO net device
    uint64_t net_device_addr = scan_for_virtio_net_device();
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

    // Send a packet and try to receive it back
    tiny_info("Sending test packet and checking for loopback\n");

    // Send test packet
    int send_result = virtio_net_send_packet(&net_dev, test_ethernet_frame, sizeof(test_ethernet_frame));
    if (send_result < 0)
    {
        tiny_error("Failed to send test packet\n");
        return -1;
    }

    // Try to receive packets
    uint8_t rx_buffer[1600];
    uint32_t received_len;
    int packets_received = 0;

    for (int i = 0; i < 100; i++)
    { // Poll for 1 second
        int result = virtio_net_receive_packet(&net_dev, rx_buffer, sizeof(rx_buffer), &received_len);
        if (result == 0)
        {
            packets_received++;
            tiny_info("Received packet: %d bytes\n", received_len);
        }

        // Small delay
        for (volatile int j = 0; j < 10000; j++)
            ;
    }

    tiny_info("Loopback test: sent 1 packet, received %d packets\n", packets_received);

    // Print final statistics
    virtio_net_print_stats(&net_dev);

    tiny_info("VirtIO Net loopback test completed\n");
    return 0;
}

int virtio_net_layered_send_test(void)
{
    tiny_info("Starting VirtIO Net layered send test\n");

    // Scan for VirtIO net device
    uint64_t net_device_addr = scan_for_virtio_net_device();
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

    // Prepare layered packet data (similar to reference implementation)
    // Layer 2: Ethernet header
    uint8_t eth_header[] = {
        // Destination MAC (broadcast)
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        // Source MAC (our test MAC)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        // EtherType (IPv4)
        0x08, 0x00};

    // Layer 3: IPv4 header
    uint8_t ip_header[] = {
        0x45, 0x00, 0x00, 0x1C, // Version, IHL, ToS, Total Length
        0x00, 0x01, 0x00, 0x00, // ID, Flags, Fragment Offset
        0x40, 0x01, 0x00, 0x00, // TTL, Protocol (ICMP), Checksum
        0xC0, 0xA8, 0x01, 0x01, // Source IP (192.168.1.1)
        0xC0, 0xA8, 0x01, 0x02  // Dest IP (192.168.1.2)
    };

    // Payload: ICMP data
    uint8_t payload[] = {
        0x08, 0x00, 0x00, 0x00, // Type (Echo Request), Code, Checksum, ID
        0x48, 0x65, 0x6C, 0x6C, // Data: "Hell"
        0x6F, 0x20, 0x4E, 0x65, // "o Ne"
        0x74, 0x21              // "t!"
    };

    // Setup buffer info array (similar to reference bi[] array)
    virtio_net_buffer_info_t buffers[3];

    // Buffer 0: Ethernet header (layer2)
    buffers[0].buffer = eth_header;
    buffers[0].size = sizeof(eth_header);
    buffers[0].flags = 0;
    buffers[0].copy = 1;

    // Buffer 1: IP header (layer3)
    buffers[1].buffer = ip_header;
    buffers[1].size = sizeof(ip_header);
    buffers[1].flags = 0;
    buffers[1].copy = 1;

    // Buffer 2: Payload
    buffers[2].buffer = payload;
    buffers[2].size = sizeof(payload);
    buffers[2].flags = 0;
    buffers[2].copy = 1;

    uint32_t total_size = sizeof(eth_header) + sizeof(ip_header) + sizeof(payload);
    tiny_info("Sending layered packet: L2=%d, L3=%d, Payload=%d (Total=%d bytes)\n",
              sizeof(eth_header), sizeof(ip_header), sizeof(payload), total_size);

    // Send layered packet
    int result = virtio_net_send_layered(&net_dev, buffers, 3);
    if (result < 0)
    {
        tiny_error("Failed to send layered packet\n");
        return -1;
    }

    tiny_info("Layered packet sent successfully: %d bytes\n", result);

    // Print statistics
    virtio_net_print_stats(&net_dev);

    tiny_info("VirtIO Net layered send test completed successfully\n");
    return 0;
}
