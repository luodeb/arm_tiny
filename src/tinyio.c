#include "tinyio.h"
#include <config.h>
#include <spin_lock.h>

spinlock_t lock;

void tiny_io_init()
{
    spinlock_init(&lock);
}

void uart_putchar(char c)
{
    volatile unsigned int *const UART0DR = (unsigned int *)UART_BASE_ADDR;
    spin_lock(&lock);
    *UART0DR = (unsigned int)c;
    spin_unlock(&lock);
}

void uart_putchar_nonlock(char c)
{
    volatile unsigned int *const UART0DR = (unsigned int *)UART_BASE_ADDR;
    *UART0DR = (unsigned int)c;
}

void _putchar(char character)
{
    uart_putchar_nonlock(character);
}
