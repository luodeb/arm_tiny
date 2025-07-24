#ifndef _VIRTIO_NET_H
#define _VIRTIO_NET_H

#include "virtio_mmio.h"

// VirtIO Net device feature bits
#define VIRTIO_NET_F_CSUM 0
#define VIRTIO_NET_F_GUEST_CSUM 1
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2
#define VIRTIO_NET_F_MTU 3
#define VIRTIO_NET_F_MAC 5
#define VIRTIO_NET_F_GUEST_TSO4 7
#define VIRTIO_NET_F_GUEST_TSO6 8
#define VIRTIO_NET_F_GUEST_ECN 9
#define VIRTIO_NET_F_GUEST_UFO 10
#define VIRTIO_NET_F_HOST_TSO4 11
#define VIRTIO_NET_F_HOST_TSO6 12
#define VIRTIO_NET_F_HOST_ECN 13
#define VIRTIO_NET_F_HOST_UFO 14
#define VIRTIO_NET_F_MRG_RXBUF 15
#define VIRTIO_NET_F_STATUS 16
#define VIRTIO_NET_F_CTRL_VQ 17
#define VIRTIO_NET_F_CTRL_RX 18
#define VIRTIO_NET_F_CTRL_VLAN 19
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#define VIRTIO_NET_F_MQ 22
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23

// VirtIO Net status bits
#define VIRTIO_NET_S_LINK_UP 1
#define VIRTIO_NET_S_ANNOUNCE 2

// VirtIO Net queue indices
#define VIRTIO_NET_RX_QUEUE 0
#define VIRTIO_NET_TX_QUEUE 1
#define VIRTIO_NET_CTRL_QUEUE 2

// VirtIO Net header flags
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_F_DATA_VALID 2
#define VIRTIO_NET_HDR_F_RSC_INFO 4

// VirtIO Net GSO types
#define VIRTIO_NET_HDR_GSO_NONE 0
#define VIRTIO_NET_HDR_GSO_TCPV4 1
#define VIRTIO_NET_HDR_GSO_UDP 3
#define VIRTIO_NET_HDR_GSO_TCPV6 4
#define VIRTIO_NET_HDR_GSO_ECN 0x80

// VirtIO Net configuration structure
typedef struct
{
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t duplex;
    uint8_t rss_max_key_size;
    uint16_t rss_max_indirection_table_length;
    uint32_t supported_hash_types;
} __attribute__((packed)) virtio_net_config_t;

// VirtIO Net header structure
typedef struct
{
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers; // Only if VIRTIO_NET_F_MRG_RXBUF
} __attribute__((packed)) virtio_net_hdr_t;

// VirtIO Net control commands
#define VIRTIO_NET_CTRL_RX 0
#define VIRTIO_NET_CTRL_MAC 1
#define VIRTIO_NET_CTRL_VLAN 2
#define VIRTIO_NET_CTRL_ANNOUNCE 3
#define VIRTIO_NET_CTRL_MQ 4
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS 5

// VirtIO Net control RX commands
#define VIRTIO_NET_CTRL_RX_PROMISC 0
#define VIRTIO_NET_CTRL_RX_ALLMULTI 1
#define VIRTIO_NET_CTRL_RX_ALLUNI 2
#define VIRTIO_NET_CTRL_RX_NOMULTI 3
#define VIRTIO_NET_CTRL_RX_NOUNI 4
#define VIRTIO_NET_CTRL_RX_NOBCAST 5

// VirtIO Net control MAC commands
#define VIRTIO_NET_CTRL_MAC_TABLE_SET 0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET 1

// VirtIO Net control status
#define VIRTIO_NET_OK 0
#define VIRTIO_NET_ERR 1

// VirtIO Net control header
typedef struct
{
    uint8_t class;
    uint8_t cmd;
} __attribute__((packed)) virtio_net_ctrl_hdr_t;

// VirtIO Net device structure
typedef struct
{
    virtio_device_t *dev;
    virtio_net_config_t config;
    uint8_t mac_addr[6];
    uint16_t status;
    uint16_t mtu;

    // RX/TX buffers
    void *rx_buffers[16]; // RX buffer pool
    void *tx_buffers[16]; // TX buffer pool
    uint32_t rx_buffer_size;
    uint32_t tx_buffer_size;

    // Statistics
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
} virtio_net_device_t;

// Network buffer structure for layered sending (similar to reference)
typedef struct
{
    const void *buffer;
    uint32_t size;
    uint8_t flags;
    uint8_t copy; // Whether to copy data or use direct reference
} virtio_net_buffer_info_t;

// Function declarations
int virtio_net_init(virtio_net_device_t *net_dev, uint64_t base_addr, uint32_t device_index);
int virtio_net_send_packet(virtio_net_device_t *net_dev, const void *data, uint32_t len);
int virtio_net_send_layered(virtio_net_device_t *net_dev, virtio_net_buffer_info_t *buffers, uint32_t buffer_count);
int virtio_net_receive_packet(virtio_net_device_t *net_dev, void *buffer, uint32_t buffer_size, uint32_t *received_len);
int virtio_net_setup_rx_buffers(virtio_net_device_t *net_dev);
int virtio_net_refill_rx_queue(virtio_net_device_t *net_dev);
void virtio_net_get_config(virtio_net_device_t *net_dev);
void virtio_net_print_stats(virtio_net_device_t *net_dev);
int virtio_net_set_mac_address(virtio_net_device_t *net_dev, const uint8_t *mac);

// Utility functions
void virtio_net_print_mac(const uint8_t *mac);
int virtio_net_is_link_up(virtio_net_device_t *net_dev);

#endif // _VIRTIO_NET_H
