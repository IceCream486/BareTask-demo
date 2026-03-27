#include "main.h"
#include "log.h"


/* usart dma send complete callback */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        log_dma_irq_handler();
    }
}