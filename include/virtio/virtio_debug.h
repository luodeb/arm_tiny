/*
 * virtio_debug.h
 *
 * Debug functions for VirtIO hang issue
 */

#ifndef VIRTIO_DEBUG_H
#define VIRTIO_DEBUG_H

#include "virtio_mmio.h"

// Debug functions
bool virtio_test_basic_access(void);
bool virtio_test_hang_points(void);

#endif // VIRTIO_DEBUG_H
