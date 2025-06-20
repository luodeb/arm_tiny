#ifndef CONFIG_H
#define CONFIG_H

// Configuration settings for the ARM Tiny project

#define CNTP_TIMER   30
#define CNTV_TIMER   27

#define GICD_BASE_ADDR  0x8000000
#define GICC_BASE_ADDR  0x8010000

#define UART_BASE_ADDR  0x09000000

// ARM virt machine VirtIO MMIO interrupt mapping
// QEMU assigns IRQ 79 for VirtIO device in slot 31 (0xa003e00)
// Formula: 16 (SGI) + 16 (PPI) + 32 (SPI base) + 15 (slot offset) = 79
#define USE_VIRTIO_IRQ 0 // Enable VirtIO IRQ handling
#define VIRTIO_IRQ_BASE 48
#define VIRTIO_IRQ_0 79
#endif // CONFIG_H
