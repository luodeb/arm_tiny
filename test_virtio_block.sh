#!/bin/bash

# Create a test disk image if it doesn't exist
if [ ! -f test_disk.img ]; then
    echo "Creating test disk image..."
    dd if=/dev/zero of=test_disk.img bs=1M count=10
    
    # Write some test data to the first sector
    echo "VirtIO Block Test Data - This is the first sector of the test disk image." | dd of=test_disk.img bs=512 count=1 conv=notrunc
fi

echo "Starting QEMU with VirtIO block device..."
echo "VirtIO block device will be mapped to address 0xa003e00"

# Run QEMU with VirtIO block device
qemu-system-aarch64 \
    -m 4G \
    -M virt \
    -cpu cortex-a72 \
    -nographic \
    -kernel build/arm_tiny.elf \
    -device virtio-blk-device,drive=hd0 \
    -drive file=test_disk.img,if=none,id=hd0,format=raw \
    -global virtio-mmio.force-legacy=on

echo "QEMU exited."
