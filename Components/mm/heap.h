/*
 * 基于 FreeRTOS 内存管理接口修改
 * Original: FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * 
 * SPDX-License-Identifier: MIT
 *
 * 本头文件定义了基于 FreeRTOS 的内存管理接口，提供了标准的 malloc/free 功能。
 */
#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存分配函数
 * * @param xWantedSize 期望分配的字节数
 * @return void* 指向分配内存的指针；若分配失败则返回 NULL
 */
void * pvPortMalloc( size_t xWantedSize );

/**
 * @brief 内存释放函数
 * * @param pv 指向要释放内存的指针（必须由 pvPortMalloc 分配）
 */
void vPortFree( void * pv );

/**
 * @brief 获取当前堆中剩余的空闲内存大小
 * * @return size_t 当前可用字节数
 */
size_t xPortGetFreeHeapSize( void );

/**
 * @brief 获取系统运行以来历史最低的空闲内存大小（水位线）
 * * @return size_t 历史最少剩余字节数。值越小说明内存压力越大。
 */
size_t xPortGetMinimumEverFreeHeapSize( void );

#ifdef __cplusplus
}
#endif

#endif /* HEAP_H */