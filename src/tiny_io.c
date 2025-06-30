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

void print_int(int num)
{
    char str[32];
    int i = 0;
    int j = 0;
    if (num < 0)
    {
        print_char('-');
        num = -num;
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

// 打印整数，支持宽度和填充字符
void print_int_format(int num, int width, char pad_char)
{
    char str[32];
    int i = 0;
    int j = 0;
    int is_negative = 0;

    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }

    // 清空缓冲区
    for (j = 0; j < 32; j++)
    {
        str[j] = 0;
    }

    // 转换数字
    while (num)
    {
        str[i++] = num % 10 + '0';
        num /= 10;
    }
    if (i == 0)
    {
        str[i++] = '0';
    }

    // 反转字符串
    for (j = 0; j < i / 2; j++)
    {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
    str[i] = '\0';

    // 计算实际需要的宽度（包括负号）
    int actual_width = i + (is_negative ? 1 : 0);

    // 如果需要填充
    if (width > actual_width)
    {
        int pad_count = width - actual_width;

        if (pad_char == '0' && is_negative)
        {
            // 对于数字填充，负号在前面
            print_char('-');
            for (j = 0; j < pad_count; j++)
            {
                print_char(pad_char);
            }
            print_str(str);
        }
        else
        {
            // 对于空格填充或正数的0填充
            if (pad_char != '0')
            {
                for (j = 0; j < pad_count; j++)
                {
                    print_char(pad_char);
                }
            }
            if (is_negative)
            {
                print_char('-');
            }
            if (pad_char == '0')
            {
                for (j = 0; j < pad_count; j++)
                {
                    print_char(pad_char);
                }
            }
            print_str(str);
        }
    }
    else
    {
        // 不需要填充
        if (is_negative)
        {
            print_char('-');
        }
        print_str(str);
    }
}

// 打印十六进制数，支持宽度、填充字符和大小写
void print_hex_format(uint32_t hex, int width, char pad_char, int uppercase)
{
    char str[32];
    int i = 0;
    int j = 0;
    char base_char = uppercase ? 'A' : 'a';

    // 清空缓冲区
    for (j = 0; j < 32; j++)
    {
        str[j] = 0;
    }

    // 转换数字
    if (hex == 0)
    {
        str[i++] = '0';
    }
    else
    {
        while (hex)
        {
            if (hex % 16 < 10)
            {
                str[i++] = hex % 16 + '0';
            }
            else
            {
                str[i++] = hex % 16 - 10 + base_char;
            }
            hex /= 16;
        }
    }

    // 反转字符串
    for (j = 0; j < i / 2; j++)
    {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
    str[i] = '\0';

    // 如果需要填充
    if (width > i)
    {
        int pad_count = width - i;
        for (j = 0; j < pad_count; j++)
        {
            print_char(pad_char);
        }
    }

    print_str(str);
}

void print_hex(uint32_t hex)
{
    print_hex_format(hex, 0, ' ', 0);
}

// 解析格式字符串中的数字
int parse_number(const char **format)
{
    int num = 0;
    while (**format >= '0' && **format <= '9')
    {
        num = num * 10 + (**format - '0');
        (*format)++;
    }
    return num;
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
    case ERROR:
        print_str("\033[31m");
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

            // 解析格式说明符
            char pad_char = ' ';
            int width = 0;

            // 检查填充字符
            if (*format == '0')
            {
                pad_char = '0';
                format++;
            }

            // 解析宽度
            if (*format >= '1' && *format <= '9')
            {
                width = parse_number(&format);
            }

            // 处理格式字符
            switch (*format)
            {
            case 'c':
                print_char(va_arg(args, uint32_t));
                break;
            case 's':
                print_str(va_arg(args, char *));
                break;
            case 'd':
                print_int_format(va_arg(args, int), width, pad_char);
                break;
            case 'p':
                print_hex_format(va_arg(args, uint32_t), width, pad_char, 0);
                break;
            case 'x':
                print_hex_format(va_arg(args, uint32_t), width, pad_char, 0);
                break;
            case 'X':
                print_hex_format(va_arg(args, uint32_t), width, pad_char, 1);
                break;
            case '%':
                print_char('%');
                break;
            default:
                // 如果遇到不认识的格式字符，就直接输出
                print_char('%');
                print_char(*format);
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
    case ERROR:
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

