#include "stream_buffer.h"
#include "heap.h"
#include <string.h>

/* 引用外部定义的任务锁，确保多任务下的同步安全 */
extern void bare_task_lock(void);
extern void bare_task_unlock(void);

StreamBufferHandle_t stream_buffer_create(uint32_t buffer_size) {
    /* 使用 pvPortMalloc 动态分配结构体内存 */
    struct stream_buffer *pxNewBuffer = (struct stream_buffer *)pvPortMalloc(sizeof(struct stream_buffer));
    if (pxNewBuffer == NULL) return NULL;

    /* 分配底层的环形缓冲区存储空间 */
    uint8_t *pucStorage = (uint8_t *)pvPortMalloc(buffer_size);
    if (pucStorage == NULL) {
        vPortFree(pxNewBuffer);
        return NULL;
    }

    /* 初始化环形缓冲区组件 */
    ring_buf_init(&pxNewBuffer->rb, buffer_size, pucStorage);
    pxNewBuffer->trigger_level = 1; // 默认有1字节就允许读取

    return pxNewBuffer;
}

uint32_t stream_buffer_send(StreamBufferHandle_t xStreamBuffer, 
                            const void * pvTxData, 
                            uint32_t xDataLengthBytes) {
    if (xStreamBuffer == NULL || pvTxData == NULL) return 0;

    uint32_t xReturn = 0;

    /* 关键：通过加锁确保写入操作的原子性，防止多任务竞争 */
    bare_task_lock();
    
    /* 调用底层 ring_buf_put 写入数据 */
    xReturn = ring_buf_put(&xStreamBuffer->rb, (const uint8_t *)pvTxData, xDataLengthBytes);
    
    bare_task_unlock();
    return xReturn;
}

uint32_t stream_buffer_receive(StreamBufferHandle_t xStreamBuffer, 
                               void * pvRxData, 
                               uint32_t xBufferLengthBytes) {
    if (xStreamBuffer == NULL || pvRxData == NULL) return 0;

    uint32_t xReturn = 0;

    bare_task_lock();
    
    /* 检查当前缓冲区内现有的字节数 */
    uint32_t bytes_available = ring_buf_size_get(&xStreamBuffer->rb);
    
    /* 只有当数据量达到触发阈值（或本次读取能读完现有数据）时才允许读取，模拟流同步 */
    if (bytes_available >= xStreamBuffer->trigger_level || bytes_available >= xBufferLengthBytes) {
        /* 调用底层 ring_buf_get 获取数据 */
        xReturn = ring_buf_get(&xStreamBuffer->rb, (uint8_t *)pvRxData, xBufferLengthBytes);
    }
    
    bare_task_unlock();
    return xReturn;
}

void stream_buffer_delete(StreamBufferHandle_t xStreamBuffer) {
    if (xStreamBuffer != NULL) {
        /* 依次释放存储空间和控制结构体内存 */
        vPortFree(xStreamBuffer->rb.buffer);
        vPortFree(xStreamBuffer);
    }
}