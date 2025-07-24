#include "virtio/virtio_net.h"
#include "tinystd.h"
#include "tinyio.h"

// Default buffer sizes
#define VIRTIO_NET_DEFAULT_RX_BUFFER_SIZE 1514 // Standard Ethernet frame size
#define VIRTIO_NET_DEFAULT_TX_BUFFER_SIZE 1514
#define VIRTIO_NET_QUEUE_SIZE 16

static void virtio_net_read_config(virtio_net_device_t *net_dev)
{
    virtio_device_t *dev = net_dev->dev;

    // Read MAC address
    for (int i = 0; i < 6; i++)
    {
        net_dev->config.mac[i] = virtio_read8(dev, VIRTIO_MMIO_CONFIG + i) & 0xFF;
        net_dev->mac_addr[i] = net_dev->config.mac[i];
    }

    // Read status
    net_dev->config.status = virtio_read16(dev, VIRTIO_MMIO_CONFIG + 6) & 0xFFFF;
    net_dev->status = net_dev->config.status;

    // Read max virtqueue pairs
    net_dev->config.max_virtqueue_pairs = virtio_read16(dev, VIRTIO_MMIO_CONFIG + 8) & 0xFFFF;

    // Read MTU
    net_dev->config.mtu = virtio_read16(dev, VIRTIO_MMIO_CONFIG + 10) & 0xFFFF;
    net_dev->mtu = net_dev->config.mtu;
    if (net_dev->mtu == 0)
    {
        net_dev->mtu = 1500; // Default MTU
    }
}

void virtio_net_get_config(virtio_net_device_t *net_dev)
{
    virtio_net_read_config(net_dev);

    tiny_info("VirtIO Net configuration:\n");
    tiny_info("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
              net_dev->mac_addr[0], net_dev->mac_addr[1], net_dev->mac_addr[2],
              net_dev->mac_addr[3], net_dev->mac_addr[4], net_dev->mac_addr[5]);
    tiny_info("  Status: 0x%x (Link %s)\n", net_dev->status,
              (net_dev->status & VIRTIO_NET_S_LINK_UP) ? "UP" : "DOWN");
    tiny_info("  MTU: %d\n", net_dev->mtu);
    tiny_info("  Max queue pairs: %d\n", net_dev->config.max_virtqueue_pairs);
}

int virtio_net_init(virtio_net_device_t *net_dev, uint64_t base_addr, uint32_t device_index)
{
    // Initialize VirtIO allocator first
    if (virtio_allocator_init() < 0)
    {
        tiny_error("Failed to initialize VirtIO allocator\n");
        return -1;
    }

    // Allocate device structure
    net_dev->dev = (virtio_device_t *)virtio_alloc(sizeof(virtio_device_t), 8);
    if (!net_dev->dev)
    {
        tiny_error("Failed to allocate device structure\n");
        return -1;
    }

    // Initialize VirtIO device
    if (virtio_mmio_init(net_dev->dev, base_addr, device_index) < 0)
    {
        tiny_error("Failed to initialize VirtIO device\n");
        return -1;
    }

    // Check device ID
    if (net_dev->dev->device_id != VIRTIO_ID_NET)
    {
        tiny_error("Device is not a VirtIO Net device (ID: %d)\n", net_dev->dev->device_id);
        return -1;
    }

    // Read device features
    virtio_write32(net_dev->dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    net_dev->dev->device_features = virtio_read32(net_dev->dev, VIRTIO_MMIO_DEVICE_FEATURES);

    tiny_info("Net device features: 0x%lx\n", net_dev->dev->device_features);

    // Set driver features (basic features for simple operation)
    net_dev->dev->driver_features = 0;
    if (net_dev->dev->device_features & (1ULL << VIRTIO_NET_F_MAC))
    {
        net_dev->dev->driver_features |= (1ULL << VIRTIO_NET_F_MAC);
        tiny_info("Enabling MAC feature\n");
    }
    if (net_dev->dev->device_features & (1ULL << VIRTIO_NET_F_STATUS))
    {
        net_dev->dev->driver_features |= (1ULL << VIRTIO_NET_F_STATUS);
        tiny_info("Enabling STATUS feature\n");
    }

    virtio_write32(net_dev->dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virtio_write32(net_dev->dev, VIRTIO_MMIO_DRIVER_FEATURES, net_dev->dev->driver_features);

    // Set features OK
    virtio_set_status(net_dev->dev, VIRTIO_STATUS_FEATURES_OK);

    // Check if device accepted our features
    if (!(virtio_get_status(net_dev->dev) & VIRTIO_STATUS_FEATURES_OK))
    {
        tiny_error("Device rejected our features\n");
        return -1;
    }

    // Setup RX queue (queue 0)
    if (virtio_queue_setup(net_dev->dev, VIRTIO_NET_RX_QUEUE, VIRTIO_NET_QUEUE_SIZE) < 0)
    {
        tiny_error("Failed to setup RX queue\n");
        return -1;
    }

    // Setup TX queue (queue 1)
    if (virtio_queue_setup(net_dev->dev, VIRTIO_NET_TX_QUEUE, VIRTIO_NET_QUEUE_SIZE) < 0)
    {
        tiny_error("Failed to setup TX queue\n");
        return -1;
    }

    // Set driver OK
    virtio_set_status(net_dev->dev, VIRTIO_STATUS_DRIVER_OK);

    // Initialize buffer sizes
    net_dev->rx_buffer_size = VIRTIO_NET_DEFAULT_RX_BUFFER_SIZE;
    net_dev->tx_buffer_size = VIRTIO_NET_DEFAULT_TX_BUFFER_SIZE;

    // Initialize statistics
    net_dev->rx_packets = 0;
    net_dev->tx_packets = 0;
    net_dev->rx_bytes = 0;
    net_dev->tx_bytes = 0;
    net_dev->rx_errors = 0;
    net_dev->tx_errors = 0;

    // Read device configuration
    virtio_net_get_config(net_dev);

    // Setup RX buffers
    if (virtio_net_setup_rx_buffers(net_dev) < 0)
    {
        tiny_error("Failed to setup RX buffers\n");
        return -1;
    }

    tiny_info("VirtIO Net device initialized successfully\n");
    return 0;
}

int virtio_net_setup_rx_buffers(virtio_net_device_t *net_dev)
{
    // Allocate RX buffers
    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE; i++)
    {
        net_dev->rx_buffers[i] = virtio_alloc(net_dev->rx_buffer_size + sizeof(virtio_net_hdr_t), 16);
        if (!net_dev->rx_buffers[i])
        {
            tiny_error("Failed to allocate RX buffer %d\n", i);
            return -1;
        }
    }

    // Add RX buffers to the receive queue
    return virtio_net_refill_rx_queue(net_dev);
}

int virtio_net_refill_rx_queue(virtio_net_device_t *net_dev)
{
    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE; i++)
    {
        if (!net_dev->rx_buffers[i])
            continue;

        uint64_t buffer_addr = (uint64_t)net_dev->rx_buffers[i];
        uint32_t buffer_len = net_dev->rx_buffer_size + sizeof(virtio_net_hdr_t);

        // Add buffer to RX queue (write-only for device)
        int desc_id = virtio_queue_add_buf(net_dev->dev, VIRTIO_NET_RX_QUEUE,
                                           &buffer_addr, &buffer_len, 0, 1);
        if (desc_id < 0)
        {
            tiny_error("Failed to add RX buffer %d to queue\n", i);
            return -1;
        }
    }

    // Notify device about new RX buffers
    virtio_queue_kick(net_dev->dev, VIRTIO_NET_RX_QUEUE);

    tiny_info("Refilled RX queue with %d buffers\n", VIRTIO_NET_QUEUE_SIZE);
    return 0;
}

int virtio_net_send_packet(virtio_net_device_t *net_dev, const void *data, uint32_t len)
{
    if (!net_dev || !net_dev->dev || !data || len == 0)
    {
        tiny_error("Invalid parameters for send packet\n");
        return -1;
    }

    // Check frame size limit (similar to reference implementation)
    if (len > 1792)
    {
        tiny_error("Frame too big: %d bytes (max 1792)\n", len);
        net_dev->tx_errors++;
        return -1;
    }

    if (len > net_dev->mtu)
    {
        tiny_error("Packet size %d exceeds MTU %d\n", len, net_dev->mtu);
        return -1;
    }

    // Prepare VirtIO Net header (similar to reference net_header)
    virtio_net_hdr_t hdr;
    hdr.flags = VIRTIO_NET_HDR_F_NEEDS_CSUM; // Set checksum flag like reference
    hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr.hdr_len = 0;
    hdr.gso_size = 0;
    hdr.csum_start = 0;
    hdr.csum_offset = len; // Set checksum offset to packet size like reference
    hdr.num_buffers = 0;

    // Use scatter-gather approach like reference implementation
    // We'll use 2 buffers: header + data (instead of copying into single buffer)
    uint64_t buffers[2];
    uint32_t lengths[2];

    // First buffer: VirtIO header
    buffers[0] = (uint64_t)&hdr;
    lengths[0] = sizeof(virtio_net_hdr_t);

    // Second buffer: packet data
    buffers[1] = (uint64_t)data;
    lengths[1] = len;

    // Add buffers to TX queue (both read-only for device)
    int desc_id = virtio_queue_add_buf(net_dev->dev, VIRTIO_NET_TX_QUEUE,
                                       buffers, lengths, 2, 0);
    if (desc_id < 0)
    {
        tiny_error("Failed to add TX buffers to queue\n");
        net_dev->tx_errors++;
        return -1;
    }

    // Notify device
    virtio_queue_kick(net_dev->dev, VIRTIO_NET_TX_QUEUE);

    // Wait for transmission to complete (polling)
    uint32_t used_len;
    int timeout = 1000; // Simple timeout counter
    while (timeout-- > 0)
    {
        int used_desc = virtio_queue_get_buf(net_dev->dev, VIRTIO_NET_TX_QUEUE, &used_len);
        if (used_desc >= 0)
        {
            // Transmission completed
            net_dev->tx_packets++;
            net_dev->tx_bytes += len;
            tiny_debug("Sent packet: %d bytes\n", len);
            return len; // Return actual data size sent (like reference)
        }
        // Small delay
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    tiny_error("TX timeout\n");
    net_dev->tx_errors++;
    return -1;
}

int virtio_net_send_layered(virtio_net_device_t *net_dev, virtio_net_buffer_info_t *buffers, uint32_t buffer_count)
{
    if (!net_dev || !net_dev->dev || !buffers || buffer_count == 0)
    {
        tiny_error("Invalid parameters for layered send\n");
        return -1;
    }

    if (buffer_count > 4)
    {
        tiny_error("Too many buffers: %d (max 4)\n", buffer_count);
        return -1;
    }

    // Calculate total size (similar to reference implementation)
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < buffer_count; i++)
    {
        if (buffers[i].size == 0)
            break; // Stop at first empty buffer
        total_size += buffers[i].size;
    }

    // Check frame size limit
    if (total_size > 1792)
    {
        tiny_error("Frame too big: %d bytes (max 1792)\n", total_size);
        net_dev->tx_errors++;
        return -1;
    }

    // Prepare VirtIO Net header (similar to reference net_header)
    virtio_net_hdr_t hdr;
    hdr.flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr.hdr_len = 0;
    hdr.gso_size = 0;
    hdr.csum_start = 0;
    hdr.csum_offset = total_size;
    hdr.num_buffers = 0;

    // Prepare scatter-gather buffers (header + data buffers)
    uint64_t sg_buffers[5]; // Header + up to 4 data buffers
    uint32_t sg_lengths[5];
    uint32_t sg_count = 0;

    // First buffer: VirtIO header
    sg_buffers[sg_count] = (uint64_t)&hdr;
    sg_lengths[sg_count] = sizeof(virtio_net_hdr_t);
    sg_count++;

    // Add data buffers
    for (uint32_t i = 0; i < buffer_count && sg_count < 5; i++)
    {
        if (buffers[i].size == 0)
            break;

        sg_buffers[sg_count] = (uint64_t)buffers[i].buffer;
        sg_lengths[sg_count] = buffers[i].size;
        sg_count++;
    }

    tiny_debug("Sending layered packet: %d buffers, %d total bytes\n", sg_count, total_size);

    // Add buffers to TX queue (all read-only for device)
    int desc_id = virtio_queue_add_buf(net_dev->dev, VIRTIO_NET_TX_QUEUE,
                                       sg_buffers, sg_lengths, sg_count, 0);
    if (desc_id < 0)
    {
        tiny_error("Failed to add layered TX buffers to queue\n");
        net_dev->tx_errors++;
        return -1;
    }

    // Notify device
    virtio_queue_kick(net_dev->dev, VIRTIO_NET_TX_QUEUE);

    // Wait for transmission to complete (polling)
    uint32_t used_len;
    int timeout = 1000;
    while (timeout-- > 0)
    {
        int used_desc = virtio_queue_get_buf(net_dev->dev, VIRTIO_NET_TX_QUEUE, &used_len);
        if (used_desc >= 0)
        {
            // Transmission completed
            net_dev->tx_packets++;
            net_dev->tx_bytes += total_size;
            tiny_debug("Sent layered packet: %d bytes\n", total_size);
            return total_size; // Return total data size sent (like reference)
        }
        // Small delay
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    tiny_error("Layered TX timeout\n");
    net_dev->tx_errors++;
    return -1;
}

int virtio_net_receive_packet(virtio_net_device_t *net_dev, void *buffer, uint32_t buffer_size, uint32_t *received_len)
{
    if (!net_dev || !net_dev->dev || !buffer || !received_len)
    {
        tiny_error("Invalid parameters for receive packet\n");
        return -1;
    }

    *received_len = 0;

    // Check for received packets
    uint32_t used_len;
    int desc_id = virtio_queue_get_buf(net_dev->dev, VIRTIO_NET_RX_QUEUE, &used_len);
    if (desc_id < 0)
    {
        // No packets available
        return -1;
    }

    // Find the corresponding RX buffer
    void *rx_buffer = NULL;
    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE; i++)
    {
        if (net_dev->rx_buffers[i])
        {
            rx_buffer = net_dev->rx_buffers[i];
            net_dev->rx_buffers[i] = NULL; // Mark as used
            break;
        }
    }

    if (!rx_buffer)
    {
        tiny_error("No RX buffer found for descriptor %d\n", desc_id);
        net_dev->rx_errors++;
        return -1;
    }

    // Parse received data
    uint8_t *packet_data = (uint8_t *)rx_buffer + sizeof(virtio_net_hdr_t);
    uint32_t packet_len = used_len - sizeof(virtio_net_hdr_t);

    if (packet_len > buffer_size)
    {
        tiny_error("Received packet too large: %d > %d\n", packet_len, buffer_size);
        net_dev->rx_errors++;
        return -1;
    }

    // Copy packet data to user buffer
    for (uint32_t i = 0; i < packet_len; i++)
    {
        ((uint8_t *)buffer)[i] = packet_data[i];
    }
    *received_len = packet_len;

    // Update statistics
    net_dev->rx_packets++;
    net_dev->rx_bytes += packet_len;

    tiny_debug("Received packet: %d bytes\n", packet_len);

    // Allocate new RX buffer to replace the used one
    void *new_rx_buffer = virtio_alloc(net_dev->rx_buffer_size + sizeof(virtio_net_hdr_t), 16);
    if (new_rx_buffer)
    {
        // Find empty slot
        for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE; i++)
        {
            if (!net_dev->rx_buffers[i])
            {
                net_dev->rx_buffers[i] = new_rx_buffer;

                // Add new buffer to RX queue
                uint64_t buffer_addr = (uint64_t)new_rx_buffer;
                uint32_t buffer_len = net_dev->rx_buffer_size + sizeof(virtio_net_hdr_t);
                virtio_queue_add_buf(net_dev->dev, VIRTIO_NET_RX_QUEUE,
                                     &buffer_addr, &buffer_len, 0, 1);
                break;
            }
        }
        // Notify device about new RX buffer
        virtio_queue_kick(net_dev->dev, VIRTIO_NET_RX_QUEUE);
    }

    return 0;
}

void virtio_net_print_stats(virtio_net_device_t *net_dev)
{
    if (!net_dev)
    {
        tiny_error("Invalid net device\n");
        return;
    }

    tiny_info("VirtIO Net Statistics:\n");
    tiny_info("  RX: %llu packets, %llu bytes, %llu errors\n",
              net_dev->rx_packets, net_dev->rx_bytes, net_dev->rx_errors);
    tiny_info("  TX: %llu packets, %llu bytes, %llu errors\n",
              net_dev->tx_packets, net_dev->tx_bytes, net_dev->tx_errors);
}

void virtio_net_print_mac(const uint8_t *mac)
{
    if (!mac)
    {
        tiny_info("MAC: (null)\n");
        return;
    }

    tiny_info("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int virtio_net_is_link_up(virtio_net_device_t *net_dev)
{
    if (!net_dev || !net_dev->dev)
    {
        return 0;
    }

    // Re-read status from device
    virtio_net_read_config(net_dev);
    return (net_dev->status & VIRTIO_NET_S_LINK_UP) ? 1 : 0;
}

int virtio_net_set_mac_address(virtio_net_device_t *net_dev, const uint8_t *mac)
{
    if (!net_dev || !net_dev->dev || !mac)
    {
        tiny_error("Invalid parameters for set MAC address\n");
        return -1;
    }

    // Check if device supports MAC address setting
    if (!(net_dev->dev->device_features & (1ULL << VIRTIO_NET_F_CTRL_MAC_ADDR)))
    {
        tiny_error("Device does not support MAC address setting\n");
        return -1;
    }

    // For simplicity, we'll just update our local copy
    // A full implementation would use the control queue
    for (int i = 0; i < 6; i++)
    {
        net_dev->mac_addr[i] = mac[i];
    }

    tiny_info("MAC address updated to: ");
    virtio_net_print_mac(net_dev->mac_addr);

    return 0;
}
