#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>
#include <time.h>
#include <string.h>

// 日志级别定义
#define LOG_LEVEL_DEBUG   0
#define LOG_LEVEL_INFO    1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_ERROR   3

// 默认日志级别 (可以在编译时通过 -DLOG_LEVEL=x 修改)
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ERROR
#endif

// ANSI 颜色代码
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GRAY    "\033[90m"

// 获取当前时间字符串
static inline void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%H:%M:%S", tm_info);
}

// 日志宏定义
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define log_debug(fmt, ...) do { \
    char ts[32]; \
    get_timestamp(ts, sizeof(ts)); \
    printf(COLOR_GRAY "[%s DEBUG] " fmt COLOR_RESET "\n", ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define log_debug(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define log_info(fmt, ...) do { \
    char ts[32]; \
    get_timestamp(ts, sizeof(ts)); \
    printf(COLOR_GREEN "[%s INFO] " fmt COLOR_RESET "\n", ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define log_info(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define log_warn(fmt, ...) do { \
    char ts[32]; \
    get_timestamp(ts, sizeof(ts)); \
    printf(COLOR_YELLOW "[%s WARN] " fmt COLOR_RESET "\n", ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define log_warn(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define log_error(fmt, ...) do { \
    char ts[32]; \
    get_timestamp(ts, sizeof(ts)); \
    printf(COLOR_RED "[%s ERROR] " fmt COLOR_RESET "\n", ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define log_error(fmt, ...) ((void)0)
#endif

// 仅保留分级日志：debug/info/warn/error

#endif // __LOGGER_H__
