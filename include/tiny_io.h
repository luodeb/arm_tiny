#ifndef __IO_H__
#define __IO_H__

#include "tiny_types.h"
#include <stdarg.h>

// 配置打印日志
#define TINY_DEBUG
typedef enum
{
    NONE,
    INFO,
    WARN,
    DEBUG,
    ERROR,
} LOG_LEVEL;

static inline uint8_t read8(const volatile void *addr) {
    return *(const volatile uint8_t *)addr;
}

static inline void write8(uint8_t value, volatile void *addr) {
    *(volatile uint8_t *)addr = value;
}

static inline uint16_t read16(const volatile void *addr) {
    return *(const volatile uint16_t *)addr;
}

static inline void write16(uint16_t value, volatile void *addr) {
    *(volatile uint16_t *)addr = value;
}


static inline uint32_t read32(const volatile void *addr) {
    return *(const volatile uint32_t *)addr;
}

static inline void write32(uint32_t value, volatile void *addr) {
    *(volatile uint32_t *)addr = value;
}

static inline uint64_t read64(const volatile void *addr) {
    return *(const volatile uint64_t *)addr;
}

static inline void write64(uint64_t value, volatile void *addr) {
    *(volatile uint64_t *)addr = value;
}

void tiny_io_init();
void tiny_printf(LOG_LEVEL level, const char *format, ...);
void tiny_hello(void);
void soft_delay_ms(int n);
void print_char(char c);

// PSCI function IDs for system shutdown
#define PSCI_SYSTEM_OFF 0x84000008

// ARM64 system shutdown function using PSCI
static inline void system_shutdown(void)
{
    tiny_printf(INFO, "Shutting down system...\n");

    // PSCI call to shutdown the system
    __asm__ volatile(
        "mov x0, %0\n\t"
        "hvc #0\n\t"
        :
        : "r"(PSCI_SYSTEM_OFF)
        : "x0");

    // If PSCI fails, try alternative method
    // Some QEMU versions support writing to this address for shutdown
    write32(0x2000, (volatile void *)0x84000008);

    // Final fallback - infinite loop
    while (1)
    {
        __asm__ volatile("wfi"); // Wait for interrupt
    }
}

#endif // __IO_H__
