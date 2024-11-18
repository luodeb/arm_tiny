#ifndef __IO_H__
#define __IO_H__

#include "tiny_types.h"
#include <stdarg.h>

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
void tiny_printf(const char *format, ...);
void info(const char *info);
void warn(const char *info);
void debug(const char *info);
void tiny_hello(void);
#endif // __IO_H__
