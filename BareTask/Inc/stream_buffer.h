#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

#include "ring_buffer.h"
#include <stdbool.h>

/* 流式缓冲区结构体 */
struct stream_buffer {
    struct ring_buf rb;        /* 底层环形缓冲区 */
    uint32_t trigger_level;    /* 触发阈值：缓冲区数据达到此字节数时才建议读取 */
};

typedef struct stream_buffer * StreamBufferHandle_t;

/**
 * @brief 创建一个流式缓冲区
 * @param buffer_size 缓冲区总容量（字节）
 * @return 缓冲区句柄，失败返回 NULL
 */
StreamBufferHandle_t stream_buffer_create(uint32_t buffer_size);

/**
 * @brief 向缓冲区写入字节流
 * @param xStreamBuffer 句柄
 * @param pvTxData 要发送的数据指针
 * @param xDataLengthBytes 拟写入的长度
 * @return 实际写入的字节数
 */
uint32_t stream_buffer_send(StreamBufferHandle_t xStreamBuffer, 
                            const void * pvTxData, 
                            uint32_t xDataLengthBytes);

/**
 * @brief 从缓冲区读取字节流
 * @param xStreamBuffer 句柄
 * @param pvRxData 存放数据的缓冲区指针
 * @param xBufferLengthBytes 拟读取的最大长度
 * @return 实际读取的字节数
 */
uint32_t stream_buffer_receive(StreamBufferHandle_t xStreamBuffer, 
                               void * pvRxData, 
                               uint32_t xBufferLengthBytes);

/**
 * @brief 销毁缓冲区
 */
void stream_buffer_delete(StreamBufferHandle_t xStreamBuffer);

#endif