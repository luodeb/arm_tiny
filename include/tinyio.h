#ifndef _TINYIO_H
#define _TINYIO_H
#include "printf.h"

typedef enum
{
    NONE,
    INFO,
    WARN,
    DEBUG,
    ERROR
} LOG_LEVEL;

#define set_color(level)        \
    do                          \
    {                           \
        switch (level)          \
        {                       \
        case INFO:              \
            printf("\033[32m"); \
            break;              \
        case WARN:              \
            printf("\033[33m"); \
            break;              \
        case DEBUG:             \
            printf("\033[34m"); \
            break;              \
        case ERROR:             \
            printf("\033[31m"); \
            break;              \
        default:                \
            printf("\033[0m");  \
            break;              \
        }                       \
    } while (0)
#define reset_color() printf("\033[0m")

#define tiny_log(level, format, ...)            \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        set_color(level);                       \
        printf(format, ##__VA_ARGS__);          \
        reset_color();                          \
    } while (0)

#endif
