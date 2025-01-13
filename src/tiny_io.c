/*
 * tiny_io.c
 *
 * Created on: 2024-11-15
 * Author: Debin
 *
 * Description: This file contains the implementation of I/O functions for the ARM Tiny project.
 */

#include "tiny_io.h"
#include <spin_lock.h>
#include <config.h>

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

void print_char(char c)
{
    uart_putchar_nonlock(c);
}

void print_str(const char *str)
{
    while (*str)
    {
        print_char(*str++);
    }
}

void print_int(uint32_t num)
{
    char str[32];
    int i = 0;
    int j = 0;
    if (num < 0)
    {
        print_char('-');
        i = 1;
    }
    for (j = 0; j < 32; j++)
    {
        str[j] = 0;
    }
    while (num)
    {
        str[i++] = num % 10 + '0';
        num /= 10;
    }
    if (i == 0)
    {
        str[i++] = '0';
    }
    str[i] = '\0';
    for (j = 0; j < i / 2; j++)
    {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    print_str(str);
}

// void print_float(float num)
// {
//     int integer = (int)num;
//     print_int(integer);
//     print_char('.');
//     int decimal = (int)((num - integer) * 1000000);
//     // 去除末尾的0
//     while (decimal % 10 == 0)
//     {
//         decimal /= 10;
//     }
//     print_int(decimal);
// }

void print_hex(uint32_t hex)
{
    char str[32];
    int i = 0;
    int j = 0;
    for (j = 0; j < 32; j++)
    {
        str[j] = 0;
    }
    str[0] = '0';
    str[1] = 'x';
    for (i = 2; i < 10; i++)
    {
        str[i] = '0';
    }
    i = 9;
    while (hex)
    {
        if (hex % 16 < 10)
        {
            str[i--] = hex % 16 + '0';
        }
        else
        {
            str[i--] = hex % 16 - 10 + 'a';
        }
        hex /= 16;
    }
    print_str(str);
}

void tiny_printf(LOG_LEVEL level, const char *format, ...)
{
    switch (level)
    {
    case INFO:
        print_str("\033[32m");
        break;
    case WARN:
        print_str("\033[33m");
        break;
    case DEBUG:
        print_str("\033[34m");
        break;

    default:
        break;
    }
    va_list args;
    va_start(args, format);
    while (*format)
    {
        if (*format == '%')
        {
            format++;
            switch (*format)
            {
            case 'c':
                print_char(va_arg(args, uint32_t));
                break;
            case 's':
                print_str(va_arg(args, char *));
                break;
            case 'd':
                print_int(va_arg(args, int));
                break;
            // case 'f':
            //     print_float(va_arg(args, double));
            //     break;
            case 'x':
                print_hex(va_arg(args, uint32_t));
                break;
            default:
                break;
            }
        }
        else
        {
            print_char(*format);
        }
        format++;
    }
    switch (level)
    {
    case INFO:
        print_str("\033[0m");
        break;
    case WARN:
        print_str("\033[0m");
        break;
    case DEBUG:
        print_str("\033[0m");
        break;

    default:
        break;
    }
    va_end(args);
}

void tiny_hello(void)
{
    tiny_printf(INFO, "Hello, ARM Tiny!\n");
}

void soft_delay(int n)
{
    for (int i = 0; i < n; i++)
    {
        asm volatile("nop");
    }
}

void soft_delay_ms(int n)
{
    for (int i = 0; i < n; i++)
    {
        soft_delay(1000000);
    }
}
