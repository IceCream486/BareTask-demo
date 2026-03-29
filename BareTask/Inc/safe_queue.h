#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include "ring_buffer.h"
#include <stdbool.h>

/* 消息队列句柄 */
struct safe_queue {
    struct ring_buf rb;      /* 底层环形缓冲区 */
    uint32_t msg_size;       /* 单个消息的固定大小 */
    uint32_t max_msgs;       /* 队列最大容纳消息数 */
};

typedef struct safe_queue * SafeQueueHandle_t;

/**
 * @brief 创建一个新的同步队列
 * @param msg_count 队列深度（消息个数）
 * @param msg_size 每个消息的大小（字节）
 * @return 队列句柄，失败返回 NULL
 */
SafeQueueHandle_t safe_queue_create(uint32_t msg_count, uint32_t msg_size);

/**
 * @brief 发送消息到队列（尾部入队）
 * @param xQueue 队列句柄
 * @param pvItemToQueue 指向要发送的数据源
 * @return true 发送成功, false 队列已满
 */
bool safe_queue_send(SafeQueueHandle_t xQueue, const void * pvItemToQueue);

/**
 * @brief 从队列接收消息（头部出队）
 * @param xQueue 队列句柄
 * @param pvBuffer 存放接收数据的缓冲区
 * @return true 接收成功, false 队列为空
 */
bool safe_queue_receive(SafeQueueHandle_t xQueue, void * pvBuffer);

/**
 * @brief 销毁队列并释放内存
 */
void safe_queue_delete(SafeQueueHandle_t xQueue);

#endif