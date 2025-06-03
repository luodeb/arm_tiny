#!/bin/bash
# 如果，mnt不存在，创建mnt
if [ ! -d "mnt" ]; then
    mkdir mnt
fi
sudo mount ../vmm0117/arceos-umhv/arceos-vmm/disk.img mnt
sudo cp -r arm_tiny_vm_0x40080000.bin arm_tiny_vm_0x80080000.bin mnt
sudo umount mnt
rm -rf mnt