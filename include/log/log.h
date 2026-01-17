#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BAPI_LOG_LEVEL_DEBUG = 0,
    BAPI_LOG_LEVEL_INFO,
    BAPI_LOG_LEVEL_WARN,
    BAPI_LOG_LEVEL_ERROR,
    BAPI_LOG_LEVEL_CRITICAL,
    BAPI_LOG_LEVEL_NONE
} bapi_log_level_t;

typedef struct {
    bapi_log_level_t min_level;
    bool use_colors;
    bool use_file;
    const char* log_file_path;
} bapi_log_config_t;

bool bapi_log_init(const bapi_log_config_t* config);
void bapi_log_shutdown(void);
void bapi_log_set_level(bapi_log_level_t level);

void bapi_log_message(bapi_log_level_t level, const char* file, int line, const char* func, const char* format, ...);

#ifdef __cplusplus
}
#endif

#if defined(BAPI_LOG_ENABLED)

#define BAPI_LOG_DEBUG(fmt, ...) \
    bapi_log_message(BAPI_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BAPI_LOG_INFO(fmt, ...) \
    bapi_log_message(BAPI_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BAPI_LOG_WARN(fmt, ...) \
    bapi_log_message(BAPI_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BAPI_LOG_ERROR(fmt, ...) \
    bapi_log_message(BAPI_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BAPI_LOG_CRITICAL(fmt, ...) \
    bapi_log_message(BAPI_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BAPI_LOG_ASSERT(condition, fmt, ...) \
    do { \
        if (!(condition)) { \
            bapi_log_message(BAPI_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __func__, \
                             "ASSERTION FAILED: " fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#else

#define BAPI_LOG_DEBUG(fmt, ...) ((void)0)
#define BAPI_LOG_INFO(fmt, ...) ((void)0)
#define BAPI_LOG_WARN(fmt, ...) ((void)0)
#define BAPI_LOG_ERROR(fmt, ...) ((void)0)
#define BAPI_LOG_CRITICAL(fmt, ...) ((void)0)
#define BAPI_LOG_ASSERT(condition, fmt, ...) ((void)(condition))

#endif

#define BAPI_LOG_INIT_DEFAULT() \
    do { \
        bapi_log_config_t _config = { \
            .min_level = BAPI_LOG_LEVEL_INFO, \
            .use_colors = true, \
            .use_file = false, \
            .log_file_path = NULL \
        }; \
        bapi_log_init(&_config); \
    } while (0)
