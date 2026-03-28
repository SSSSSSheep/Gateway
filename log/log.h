/**
 * @file log.h
 * @brief 简单的日志模块头文件
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>

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
 * @brief 设置日志级别
 */
void log_set_level(log_level_t level);

/**
 * @brief 通用日志函数
 */
void log_log(log_level_t level, const char *file, int line, const char *fmt, ...);

/**
 * @brief 日志宏定义
 */
#define log_debug(fmt, ...) log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) log_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) log_log(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // __LOG_H__
