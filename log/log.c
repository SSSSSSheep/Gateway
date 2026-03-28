/**
 * @file log.c
 * @brief 简单的日志模块实现
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/**
 * @brief 日志级别
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

/**
 * @brief 当前日志级别
 */
static log_level_t current_log_level = LOG_LEVEL_DEBUG;

/**
 * @brief 设置日志级别
 */
void log_set_level(log_level_t level)
{
    current_log_level = level;
}

/**
 * @brief 获取当前时间字符串
 */
static void get_time_str(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief 通用日志函数
 */
void log_log(log_level_t level, const char *file, int line, const char *fmt, ...)
{
    if (level < current_log_level)
    {
        return;
    }

    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    char time_buf[32];
    get_time_str(time_buf, sizeof(time_buf));

    printf("[%s] [%s] [%s:%d] ", time_buf, level_str[level], file, line);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
