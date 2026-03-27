#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Logging Configuration ========== */

/* Enable or disable the global log system */
#define LOG_GLOBAL_ENABLE    1
/* The minimum log level to be printed */
#define MIN_LOG_LEVEL        LOG_LEVEL_DEBUG
/* Enable or disable the timestamp in the log message */
#define LOG_TIMESTAMP_ENABLE 1
/* Total size of the circular buffer for DMA/Background processing. */
#define LOG_BUFFER_SIZE      1024
/* Maximum length of a single formatted log message. */
#define LOG_MAX_MSG_LEN      256

/* ========== Data Types ========== */

/**
 * @brief Log severity levels
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
} LogLevel;

/* ========== Public API Macros ========== */

#if LOG_GLOBAL_ENABLE
    #define LOG_DEBUG(fmt, ...) log_output(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)  log_output(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  log_output(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...) log_output(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
    #define LOG_INFO(fmt, ...)  ((void)0)
    #define LOG_WARN(fmt, ...)  ((void)0)
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

/* ========== Function Prototypes ========== */

/**
 * @brief Initialize the log system, including DMA and buffer management.
 */
void log_init(void);

/**
 * @brief Output a formatted log message to the console.
 */
void log_output(LogLevel level, const char *fmt, ...);

/**
 * @brief Must be called in DMA Transfer Complete ISR
 */
void log_dma_irq_handler(void);


#ifdef __cplusplus
}
#endif

#endif // __LOG_H__