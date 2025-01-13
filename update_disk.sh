#!/bin/bash

sudo mount ../vmm0107/arceos-umhv/arceos-vmm/disk.img mnt
sudo cp -r arm_tiny_vm_0x40080000.bin arm_tiny_vm_0x80080000.bin mnt
sudo umount mnt