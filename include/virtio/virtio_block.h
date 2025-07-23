#ifndef _VIRTIO_BLOCK_H
#define _VIRTIO_BLOCK_H

#include "virtio_mmio.h"

// VirtIO Block device feature bits
#define VIRTIO_BLK_F_SIZE_MAX 1
#define VIRTIO_BLK_F_SEG_MAX 2
#define VIRTIO_BLK_F_GEOMETRY 4
#define VIRTIO_BLK_F_RO 5
#define VIRTIO_BLK_F_BLK_SIZE 6
#define VIRTIO_BLK_F_FLUSH 9
#define VIRTIO_BLK_F_TOPOLOGY 10
#define VIRTIO_BLK_F_CONFIG_WCE 11
#define VIRTIO_BLK_F_DISCARD 13
#define VIRTIO_BLK_F_WRITE_ZEROES 14

// VirtIO Block request types
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13

// VirtIO Block status codes
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

// VirtIO Block configuration structure
typedef struct
{
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct
    {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct
    {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0[3];
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
} __attribute__((packed)) virtio_blk_config_t;

// VirtIO Block request header
typedef struct
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) virtio_blk_req_t;

// VirtIO Block device structure
typedef struct
{
    virtio_device_t *dev;
    virtio_blk_config_t config;
    uint32_t block_size;
    uint64_t capacity;
} virtio_blk_device_t;

// Function declarations
int virtio_blk_init(virtio_blk_device_t *blk_dev, uint64_t base_addr, uint32_t device_index);
int virtio_blk_read_sector(virtio_blk_device_t *blk_dev, uint64_t sector,
                           void *buffer, uint32_t count);
int virtio_blk_write_sector(virtio_blk_device_t *blk_dev, uint64_t sector,
                            const void *buffer, uint32_t count);
void virtio_blk_get_config(virtio_blk_device_t *blk_dev);

#endif // _VIRTIO_BLOCK_H
