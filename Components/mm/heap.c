/*
 * 基于 FreeRTOS 内存管理模块修改
 * Original: FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * 
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * 本文件基于 FreeRTOS 的内存管理模块进行修改和优化，适用于嵌入式系统。
 * 修改内容包括：去除了 FreeRTOS 特定依赖、简化了配置选项、增加了中文注释等。
 */

#include <stdlib.h>
#include <string.h>
#include "heap.h"

/**
 * @brief 堆内存的总大小（字节）。
 * 用户应根据 MCU 的 RAM 资源和应用需求调整此值。
 */
#define configTOTAL_HEAP_SIZE               ( ( size_t ) 40960 )

/**
 * @brief 内存对齐字节数。
 * 必须是 2 的幂。通常 32 位系统设为 4 或 8
 */
#define portBYTE_ALIGNMENT                  4

/**
 * @brief 堆空间分配方式。
 * 0: 由本模块静态定义一个大的 uint8_t 数组作为堆池。
 * 1: 用户在外部定义名为 ucHeap 的数组（方便通过链接脚本定位到特定的 RAM 段）。
 */
#define configAPPLICATION_ALLOCATED_HEAP    0

/**
 * @brief 释放内存时是否自动清零。
 * 1: vPortFree 时将用户区清零，增加安全性，防止敏感数据残留，也方便调试。
 */
#define configHEAP_CLEAR_MEMORY_ON_FREE     1

/**
 * @brief 断言宏。
 * 用于捕捉致命错误（如释放了已被释放的内存）。生产环境下可改为复位指令。
 */
#define configASSERT( x )                   if( ( x ) == 0 ) { for( ;; ); }


/* 线程安全锁定机制：在多线程/抢占式环境下，需在此填入关中断或获取互斥锁的代码 */
#define HEAP_LOCK()     
#define HEAP_UNLOCK()   

/* 字节对齐遮罩：用于计算对齐后的地址 */
#define portBYTE_ALIGNMENT_MASK             ( portBYTE_ALIGNMENT - 1 )

/* 最小空闲块大小：防止链表中出现过小的内存碎片。若分裂后的块小于此值，则不分裂 */
#define heapMINIMUM_BLOCK_SIZE              ( ( size_t ) ( xHeapStructSize << 1 ) )

/* 状态位：利用 size_t 的最高位标记该块是否已被分配（1:已分配，0:空闲） */
#define heapBLOCK_ALLOCATED_BITMASK         ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * 8 ) - 1 ) )

/* 检查块是否已分配 */
#define heapBLOCK_IS_ALLOCATED( pxBlock )   ( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )

/* 标记块为已分配状态 */
#define heapALLOCATE_BLOCK( pxBlock )       ( ( pxBlock->xBlockSize ) |= heapBLOCK_ALLOCATED_BITMASK )

/* 标记块为空闲状态 */
#define heapFREE_BLOCK( pxBlock )           ( ( pxBlock->xBlockSize ) &= ~heapBLOCK_ALLOCATED_BITMASK )


/* --- 数据结构 --- */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /**< 指向链表中下一个空闲块 */
    size_t xBlockSize;                     /**< 当前块的大小（包含 Header 本身） */
} BlockLink_t;

/* --- 全局变量 --- */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif

/* 结构体 Header 实际占用的对齐后大小 */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + portBYTE_ALIGNMENT_MASK ) & ~portBYTE_ALIGNMENT_MASK;

static BlockLink_t xStart;           /* 链表头指针（地址最低端） */
static BlockLink_t * pxEnd = NULL;   /* 链表尾指针（地址最高端） */

static size_t xFreeBytesRemaining = 0U;             /* 当前可用总字节数 */
static size_t xMinimumEverFreeBytesRemaining = 0U;  /* 历史最低可用字节数（水位线） */
static size_t xNumberOfSuccessfulAllocations = 0U;  /* 成功分配次数计数 */
static size_t xNumberOfSuccessfulFrees = 0U;        /* 成功释放次数计数 */


/**
 * @brief 将一个空闲块插入空闲链表。
 * 链表按内存地址从小到大排序，插入后会自动检查并合并前后相邻的空闲空间。
 */
static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert )
{
    BlockLink_t * pxIterator;
    uint8_t * puc;

    /* 寻找插入位置 */
    for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock ) {}

    /* 检查是否能与前面的块合并 */
    puc = ( uint8_t * ) pxIterator;
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;
    }

    /* 检查是否能与后面的块合并 */
    puc = ( uint8_t * ) pxBlockToInsert;
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
    {
        if( pxIterator->pxNextFreeBlock != pxEnd )
        {
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    if( pxIterator != pxBlockToInsert )
    {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

/**
 * @brief 初始化堆池。
 * 整理 ucHeap 数组，设置链表头尾指针。
 */
static void prvHeapInit( void )
{
    BlockLink_t * pxFirstFreeBlock;
    uintptr_t uxStartAddress, uxEndAddress;
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;

    uxStartAddress = ( uintptr_t ) ucHeap;

    /* 确保堆池起始地址对齐 */
    if( ( uxStartAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        uxStartAddress += ( portBYTE_ALIGNMENT - 1 );
        uxStartAddress &= ~( ( uintptr_t ) portBYTE_ALIGNMENT_MASK );
        xTotalHeapSize -= ( size_t ) ( uxStartAddress - ( uintptr_t ) ucHeap );
    }

    xStart.pxNextFreeBlock = ( void * ) uxStartAddress;
    xStart.xBlockSize = ( size_t ) 0;

    uxEndAddress = uxStartAddress + ( uintptr_t ) xTotalHeapSize;
    uxEndAddress -= ( uintptr_t ) xHeapStructSize;
    uxEndAddress &= ~( ( uintptr_t ) portBYTE_ALIGNMENT_MASK );
    pxEnd = ( BlockLink_t * ) uxEndAddress;
    pxEnd->xBlockSize = 0;
    pxEnd->pxNextFreeBlock = NULL;

    pxFirstFreeBlock = ( BlockLink_t * ) uxStartAddress;
    pxFirstFreeBlock->xBlockSize = ( size_t ) ( uxEndAddress - uxStartAddress );
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
}

/* --- 公共接口实现 --- */

void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock, * pxPreviousBlock, * pxNewBlockLink;
    void * pvReturn = NULL;

    if( xWantedSize > 0 )
    {
        /* 加上 Header 的开销并进行对齐 */
        xWantedSize += xHeapStructSize;
        if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
        {
            xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
        }
    }

    HEAP_LOCK();
    {
        if( pxEnd == NULL ) { prvHeapInit(); }

        if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
        {
            pxPreviousBlock = &xStart;
            pxBlock = xStart.pxNextFreeBlock;

            /* 寻找第一个足够大的空闲块（First Fit） */
            while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
            {
                pxPreviousBlock = pxBlock;
                pxBlock = pxBlock->pxNextFreeBlock;
            }

            if( pxBlock != pxEnd )
            {
                pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );
                pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                /* 如果剩余空间足够大，则分裂该块 */
                if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                {
                    pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                    pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                    pxBlock->xBlockSize = xWantedSize;
                    prvInsertBlockIntoFreeList( pxNewBlockLink );
                }

                xFreeBytesRemaining -= pxBlock->xBlockSize;
                if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                {
                    xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                }

                heapALLOCATE_BLOCK( pxBlock ); /* 标记为已分配 */
                pxBlock->pxNextFreeBlock = NULL;
                xNumberOfSuccessfulAllocations++;
            }
        }
    }
    HEAP_UNLOCK();

    return pvReturn;
}

void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;
    BlockLink_t * pxLink;

    if( pv != NULL )
    {
        puc -= xHeapStructSize;
        pxLink = ( void * ) puc;

        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            heapFREE_BLOCK( pxLink );

            #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )
            {
                memset( pv, 0, pxLink->xBlockSize - xHeapStructSize );
            }
            #endif

            HEAP_LOCK();
            {
                xFreeBytesRemaining += pxLink->xBlockSize;
                prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                xNumberOfSuccessfulFrees++;
            }
            HEAP_UNLOCK();
        }
    }
}

size_t xPortGetFreeHeapSize( void ) { return xFreeBytesRemaining; }
size_t xPortGetMinimumEverFreeHeapSize( void ) { return xMinimumEverFreeBytesRemaining; }

