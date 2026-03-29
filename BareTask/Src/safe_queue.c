#include "safe_queue.h"
#include "heap.h"
#include <string.h>

/* 引用你系统中定义的全局锁 */
extern void bare_task_lock(void);
extern void bare_task_unlock(void);

SafeQueueHandle_t safe_queue_create(uint32_t msg_count, uint32_t msg_size) {
    /* 1. 计算总内存：队列控制结构 + 环形缓冲区空间 */
    uint32_t storage_size = msg_count * msg_size;
    
    /* 申请内存 */
    struct safe_queue *pxNewQueue = (struct safe_queue *)pvPortMalloc(sizeof(struct safe_queue));
    if (pxNewQueue == NULL) return NULL;

    uint8_t *pucQueueStorage = (uint8_t *)pvPortMalloc(storage_size);
    if (pucQueueStorage == NULL) {
        vPortFree(pxNewQueue);
        return NULL;
    }

    /* 2. 初始化底层环形缓冲区 */
    ring_buf_init(&pxNewQueue->rb, storage_size, pucQueueStorage);
    pxNewQueue->msg_size = msg_size;
    pxNewQueue->max_msgs = msg_count;

    return pxNewQueue;
}

bool safe_queue_send(SafeQueueHandle_t xQueue, const void * pvItemToQueue) {
    if (xQueue == NULL || pvItemToQueue == NULL) return false;

    bool result = false;
    
    /* 临界区保护：保证多任务并发发送时的原子性 */
    bare_task_lock();
    
    /* 检查剩余空间 */
    if (ring_buf_space_get(&xQueue->rb) >= xQueue->msg_size) {
        /* 压入数据 */
        uint32_t written = ring_buf_put(&xQueue->rb, (const uint8_t *)pvItemToQueue, xQueue->msg_size);
        if (written == xQueue->msg_size) {
            result = true;
        }
    }
    
    bare_task_unlock();
    return result;
}

bool safe_queue_receive(SafeQueueHandle_t xQueue, void * pvBuffer) {
    if (xQueue == NULL || pvBuffer == NULL) return false;

    bool result = false;

    bare_task_lock();
    
    /* 检查是否有完整消息 */
    if (ring_buf_size_get(&xQueue->rb) >= xQueue->msg_size) {
        /* 提取数据 */
        uint32_t read = ring_buf_get(&xQueue->rb, (uint8_t *)pvBuffer, xQueue->msg_size);
        if (read == xQueue->msg_size) {
            result = true;
        }
    }
    
    bare_task_unlock();
    return result;
}

void safe_queue_delete(SafeQueueHandle_t xQueue) {
    if (xQueue != NULL) {
        /* 释放缓冲区和结构体 */
        vPortFree(xQueue->rb.buffer);
        vPortFree(xQueue);
    }
}