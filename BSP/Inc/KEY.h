#ifndef KEY_H
#define KEY_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 按键逻辑事件枚举 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESSED,        /* 单击确认 */
    KEY_EVENT_DOUBLE_CLICK,   /* 双击确认 */
    KEY_EVENT_LONG_PRESSED,   /* 长按确认 */
} KEY_Event_t;

/* 按键 ID 枚举 */
typedef enum {
    KEY_ID_2 = 0,
    KEY_ID_3,
    KEY_MAX_NUM
} KEY_ID_t;

/* 初始化按键模块 */
void KEY_Init(void);

/* 获取特定按键的最后一次事件 (调用后会清除该事件) */
KEY_Event_t KEY_GetEvent(KEY_ID_t key_id);

/* 供 BareTask 调用的扫描任务 */
void KEY_Scan_Task(void* arg);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */