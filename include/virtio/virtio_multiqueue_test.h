#ifndef __VIRTIO_MULTIQUEUE_TEST_H__
#define __VIRTIO_MULTIQUEUE_TEST_H__

#include "tiny_types.h"

// Test function declarations
bool virtio_test_multiqueue_allocation(void);
bool virtio_test_multiqueue_memory_isolation(void);
bool virtio_test_backward_compatibility(void);
bool virtio_test_multiqueue_functionality(void);

#endif // __VIRTIO_MULTIQUEUE_TEST_H__
