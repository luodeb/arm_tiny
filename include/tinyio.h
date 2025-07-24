#ifndef _TINYIO_H
#define _TINYIO_H
#include "printf.h"

// Log level definitions
#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_ALL 5

// Default log level if not defined
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

// Log level names for output
#define LOG_NAME_ERROR "ERROR"
#define LOG_NAME_WARN "WARN "
#define LOG_NAME_INFO "INFO "
#define LOG_NAME_DEBUG "DEBUG"

// Base logging macro with level check
#define _tiny_log_base(level, level_name, color_start, color_end, format, ...) \
    do                                                                         \
    {                                                                          \
        if (LOG_LEVEL >= level)                                                \
        {                                                                      \
            printf("[%s][%s:%d] " color_start format color_end,                \
                   level_name, __FILE__, __LINE__, ##__VA_ARGS__);             \
        }                                                                      \
    } while (0)

// Individual log level macros
#define tiny_error(format, ...) \
    _tiny_log_base(LOG_LEVEL_ERROR, LOG_NAME_ERROR, "\033[31m", "\033[0m", format, ##__VA_ARGS__)

#define tiny_warn(format, ...) \
    _tiny_log_base(LOG_LEVEL_WARN, LOG_NAME_WARN, "\033[33m", "\033[0m", format, ##__VA_ARGS__)

#define tiny_info(format, ...) \
    _tiny_log_base(LOG_LEVEL_INFO, LOG_NAME_INFO, "\033[32m", "\033[0m", format, ##__VA_ARGS__)

#define tiny_debug(format, ...) \
    _tiny_log_base(LOG_LEVEL_DEBUG, LOG_NAME_DEBUG, "\033[34m", "\033[0m", format, ##__VA_ARGS__)

// Generic log macro (no color, always enabled if LOG_LEVEL > NONE)
#define tiny_log(format, ...) \
    _tiny_log_base(LOG_LEVEL_ERROR, "LOG  ", "", "", format, ##__VA_ARGS__)

#endif
