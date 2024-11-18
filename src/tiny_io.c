/*
 * tiny_io.c
 *
 * Created on: 2024-11-15
 * Author: Debin
 *
 * Description: This file contains the implementation of I/O functions for the ARM Tiny project.
 */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "tiny_io.h"
#include <spin_lock.h>

spinlock_t lock;

void tiny_io_init()
{
    spinlock_init(&lock);
}

void uart_putchar(char c)
{
    volatile unsigned int *const UART0DR = (unsigned int *)0x9000000;
    // spin_lock(&lock);
    *UART0DR = (unsigned int)c;
    // spin_unlock(&lock);
}

void uart_putstr(const char *str)
{
    while (*str)
    {
        uart_putchar(*str++);
    }
}

void info(const char *info)
{
    // ANSI 转义序列: "\033[32m" 设置绿色前景色, "\033[0m" 重置颜色
    tiny_printf("\033[32m%s\033[0m", info);
}

void warn(const char *info)
{
    // ANSI 转义序列: "\033[33m" 设置绿色前景色, "\033[0m" 重置颜色
    tiny_printf("\033[33m%s\033[0m", info);
}

void debug(const char *info)
{
    // ANSI 转义序列: "\033[34m" 设置绿色前景色, "\033[0m" 重置颜色
    tiny_printf("\033[34m%s\033[0m", info);
}

int int_to_str(long num, char *str, int base)
{
    if (base < 2 || base > 36)
    {
        str[0] = '\0';
        return 0;
    }

    int i = 0;
    int is_negative = (num < 0 && base == 10);

    if (is_negative)
    {
        num = -num;
    }

    do
    {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    } while (num /= base);

    if (is_negative)
    {
        str[i++] = '-';
    }

    str[i] = '\0';

    // 反转字符串
    for (int start = 0, end = i - 1; start < end; start++, end--)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
    return i;
}

int tiny_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    size_t i = 0;
    const char *p = format;

    while (*p && i < size - 1)
    {
        if (*p != '%')
        {
            str[i++] = *p++;
            continue;
        }

        p++; // 跳过 '%'
        switch (*p)
        {
        case 'd':
        {
            int val = va_arg(ap, long);
            char buf[20];
            int len = int_to_str(val, buf, 10);
            for (int j = 0; j < len && i < size - 1; j++)
            {
                str[i++] = buf[j];
            }
            break;
        }
        case 'x':
        {
            int val = va_arg(ap, int);
            char buf[20];
            int len = int_to_str(val, buf, 16);
            for (int j = 0; j < len && i < size - 1; j++)
            {
                str[i++] = buf[j];
            }
            break;
        }
        case 's':
        {
            char *val = va_arg(ap, char *);
            while (*val && i < size - 1)
            {
                str[i++] = *val++;
            }
            break;
        }
        default:
            str[i++] = *p;
            break;
        }
        p++;
    }

    str[i] = '\0';
    return i;
}

void tiny_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    uint16_t len = tiny_vsnprintf(NULL, 0, format, args);
    va_end(args);

    char buffer[len + 1];

    va_start(args, format);
    tiny_vsnprintf(buffer, len + 1, format, args);
    va_end(args);

    uart_putstr(buffer);
}

void tiny_hello(void)
{
    tiny_printf("Hello, ARM Tiny!\n");
}
