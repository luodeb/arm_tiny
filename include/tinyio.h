#ifndef _TINYIO_H
#define _TINYIO_H
#include "printf.h"

#define tiny_log(format, ...)                   \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(format, ##__VA_ARGS__);          \
    } while (0)

#define tiny_warn(format, ...)                  \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf("\033[33m");                     \
        printf(format, ##__VA_ARGS__);          \
        printf("\033[0m");                      \
    } while (0)

#define tiny_info(format, ...)                  \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf("\033[32m");                     \
        printf(format, ##__VA_ARGS__);          \
        printf("\033[0m");                      \
    } while (0)

#define tiny_debug(format, ...)                 \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf("\033[34m");                     \
        printf(format, ##__VA_ARGS__);          \
        printf("\033[0m");                      \
    } while (0)

#define tiny_error(format, ...)                 \
    do                                          \
    {                                           \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf("\033[31m");                     \
        printf(format, ##__VA_ARGS__);          \
        printf("\033[0m");                      \
    } while (0)

#endif
