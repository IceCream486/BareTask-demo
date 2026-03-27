#include "log.h"
#include "ring_buffer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* --- External Interfaces user need to implement --- */
extern uint32_t log_platform_get_time_ms(void);
extern void     log_platform_lock(void);
extern void     log_platform_unlock(void);
extern void     log_platform_dma_send(const uint8_t *data, uint16_t len);
extern bool     log_platform_dma_busy(void);

/* Static ring buffer instance */
RING_BUF_DECLARE(log_ring_buf, LOG_BUFFER_SIZE);

/* The number of bytes currently handled by the hardware DMA */
static uint32_t g_current_dma_size = 0;

void log_init(void) {
    ring_buf_reset(&log_ring_buf);
    g_current_dma_size = 0;
}

/**
 * @brief Helper function to trigger the next available DMA transfer.
 * @note Must be called within a critical section or from DMA ISR to ensure 
 * g_current_dma_size is updated atomically.
 */
static void trigger_next_dma_transfer(void) {
    /* 1. Check if hardware is busy */
    if (log_platform_dma_busy()) {
        return;
    }

    uint8_t *dma_ptr;
    /* 2. Claim contiguous block from ring buffer */
    uint32_t size = ring_buf_get_claim(&log_ring_buf, &dma_ptr, LOG_BUFFER_SIZE);
    
    if (size > 0) {
        /* 3. Record the size so we can 'finish' it in the ISR later */
        g_current_dma_size = size;
        /* 4. Start hardware transfer */
        log_platform_dma_send(dma_ptr, (uint16_t)size);
    }
}

/**
 * @brief Output a formatted log message (Thread-safe).
 */
void log_output(LogLevel level, const char *fmt, ...) {
    /* 1. Filter by log level */
    if (level < MIN_LOG_LEVEL) {
        return;
    }

    /* Format on stack to ensure reentrancy safety */
    char temp_msg[LOG_MAX_MSG_LEN];
    int len = 0;

    /* 2. Format Timestamp */
#if LOG_TIMESTAMP_ENABLE
    len += snprintf(temp_msg + len, LOG_MAX_MSG_LEN - len, "[%u]", log_platform_get_time_ms());
#endif

    /* 3. Format Level Tag */
    static const char *level_tags[] = {"DBG", "INFO", "WARN", "ERR"};
    len += snprintf(temp_msg + len, LOG_MAX_MSG_LEN - len, "[%s]: ", level_tags[level]);

    /* 4. Format User Message */
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(temp_msg + len, LOG_MAX_MSG_LEN - len, fmt, args);
    va_end(args);

    /* 5. Add Newline and Termination */
    if (len < LOG_MAX_MSG_LEN - 2) {
        strcat(temp_msg, "\r\n");
        len += 2;
    } else {
        temp_msg[LOG_MAX_MSG_LEN - 3] = '\r';
        temp_msg[LOG_MAX_MSG_LEN - 2] = '\n';
        temp_msg[LOG_MAX_MSG_LEN - 1] = '\0';
        len = LOG_MAX_MSG_LEN - 1;
    }

    log_platform_lock();

    /* 6. Push to buffer */
    ring_buf_put(&log_ring_buf, (uint8_t *)temp_msg, (uint32_t)len);

    /* 7. Try to trigger transfer. 
       We use the helper to ensure g_current_dma_size is updated correctly. */
    trigger_next_dma_transfer();

    log_platform_unlock();
}

/**
 * @brief Call this in your DMA Transfer Complete Interrupt Handler
 */
void log_dma_irq_handler(void) {
    /* 1. Mark the previous claim as finished using the saved size */
    if (g_current_dma_size > 0) {
        ring_buf_get_finish(&log_ring_buf, g_current_dma_size);
        g_current_dma_size = 0;
    }

    /* 2. Start the next transfer if buffer is not empty */
    trigger_next_dma_transfer();
}