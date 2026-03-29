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

/* init the key module */
void KEY_Init(void);

/* get the key event */
KEY_Event_t KEY_GetEvent(KEY_ID_t key_id);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */