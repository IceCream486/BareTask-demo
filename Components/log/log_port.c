#include "log.h"
#include "ring_buffer.h"
#include "main.h"

extern UART_HandleTypeDef huart1;
#define LOG_UART_HANDLE huart1

/**
 * @brief Get system tick in milliseconds
 */
uint32_t log_platform_get_time_ms(void) {
    return HAL_GetTick();
}

/**
 * @brief Check if DMA is busy transmitting
 */
bool log_platform_dma_busy(void) {
    return (HAL_UART_GetState(&LOG_UART_HANDLE) == HAL_UART_STATE_BUSY_TX || 
            HAL_UART_GetState(&LOG_UART_HANDLE) == HAL_UART_STATE_BUSY_TX_RX);
}

/**
 * @brief Starts the actual hardware DMA transfer
 * @note We use the contiguous pointer provided by ring_buf_get_claim
 */
void log_platform_dma_send(const uint8_t *data, uint16_t len) {
    if (len == 0) return;
    
    if (HAL_UART_Transmit_DMA(&LOG_UART_HANDLE, (uint8_t *)data, len) != HAL_OK) {

    }
}

static uint32_t cpu_sr;

void log_platform_lock(void) {
    cpu_sr = __get_PRIMASK();
    __disable_irq();
}

void log_platform_unlock(void) {
    __set_PRIMASK(cpu_sr);
}