#include "KEY.h"
#include "smf.h"
#include "heap.h"
#include "bare_task.h"
#include <stdint.h>
#include "log.h"

extern uint32_t bare_task_get_ms(void);
static void KEY_Scan_Task(void* arg);

/* --- 配置参数 --- */
#define TIME_LONG_PRESS_MS    500  /* 长按判定：100ms */
#define TIME_DOUBLE_TAP_MS    200   /* 双击间隔：200ms */
#define SCAN_PERIOD_MS        10    /* 扫描周期：20ms */

/* --- 内部状态索引 --- */
enum {
    S_IDLE,           /* 空闲：等待按下 */
    S_PRESS_CHECK,    /* 首次按下：区分长按、松开 */
    S_WAIT_DOUBLE,    /* 等待双击：第一次松开后等待第二次按下 */
    S_LONG_PRESSED    /* 长按中：等待物理松开 */
};

/* --- 按键对象结构体 --- */
typedef struct {
    struct smf_ctx ctx;          /* SMF 状态机上下文 */
    GPIO_TypeDef* port;          /* STM32 端口 */
    uint16_t pin;                /* STM32 引脚 */
    uint32_t timer_start;        /* 计时器起始点 */
    bool is_physically_low;      /* 物理电平是否为低 (按下) */
    KEY_Event_t event_out;       /* 输出给应用的事件 */
} KEY_Obj_t;

/* 【重要】前向声明状态表，否则函数内无法引用 key_fsm */
static const struct smf_state key_fsm[];

static KEY_Obj_t* g_keys[KEY_MAX_NUM] = {NULL};

/* --- 状态机动作函数 --- */

// 1. IDLE: 监控按下
static void state_idle_run(void *obj) {
    KEY_Obj_t *k = (KEY_Obj_t *)obj;
    if (k->is_physically_low) {
        k->timer_start = bare_task_get_ms();
        smf_set_state(SMF_CTX(k), &key_fsm[S_PRESS_CHECK]);
    }
}

// 2. PRESS_CHECK: 判定长按或松开
static void state_press_check_run(void *obj) {
    KEY_Obj_t *k = (KEY_Obj_t *)obj;
    uint32_t now = bare_task_get_ms();

    if (!k->is_physically_low) {
        /* 100ms内松开，可能触发双击，进入等待 */
        k->timer_start = now; 
        smf_set_state(SMF_CTX(k), &key_fsm[S_WAIT_DOUBLE]);
    } else if ((now - k->timer_start) >= TIME_LONG_PRESS_MS) {
        /* 达到长按阈值 */
        k->event_out = KEY_EVENT_LONG_PRESSED;
        smf_set_state(SMF_CTX(k), &key_fsm[S_LONG_PRESSED]);
    }
}

// 3. WAIT_DOUBLE: 等待第二次按下
static void state_wait_double_run(void *obj) {
    KEY_Obj_t *k = (KEY_Obj_t *)obj;
    uint32_t now = bare_task_get_ms();

    if (k->is_physically_low) {
        /* 在200ms内再次按下，确认双击 */
        k->event_out = KEY_EVENT_DOUBLE_CLICK;
        smf_set_state(SMF_CTX(k), &key_fsm[S_LONG_PRESSED]); 
    } else if ((now - k->timer_start) >= TIME_DOUBLE_TAP_MS) {
        /* 超过时间没按下，确认只是单击 */
        k->event_out = KEY_EVENT_PRESSED;
        smf_set_state(SMF_CTX(k), &key_fsm[S_IDLE]);
    }
}

// 4. LONG_PRESSED: 等待最终物理松开
static void state_long_press_run(void *obj) {
    KEY_Obj_t *k = (KEY_Obj_t *)obj;
    if (!k->is_physically_low) {
        smf_set_state(SMF_CTX(k), &key_fsm[S_IDLE]);
    }
}

/* 状态表定义 (使用强制转换匹配 SMF 库的函数指针类型) */
static const struct smf_state key_fsm[] = {
    [S_IDLE]         = SMF_CREATE_STATE(NULL, (state_execution)state_idle_run, NULL, NULL, NULL),
    [S_PRESS_CHECK]  = SMF_CREATE_STATE(NULL, (state_execution)state_press_check_run, NULL, NULL, NULL),
    [S_WAIT_DOUBLE]  = SMF_CREATE_STATE(NULL, (state_execution)state_wait_double_run, NULL, NULL, NULL),
    [S_LONG_PRESSED] = SMF_CREATE_STATE(NULL, (state_execution)state_long_press_run, NULL, NULL, NULL),
};

/* --- 公开接口 --- */

void KEY_Init(void) {

    /* 为两个按键分配内存 */
    for (int i = 0; i < KEY_MAX_NUM; i++) {
        g_keys[i] = (KEY_Obj_t*)pvPortMalloc(sizeof(KEY_Obj_t));
        if (g_keys[i]) {
            g_keys[i]->port = GPIOA;
            g_keys[i]->pin = (i == KEY_ID_2) ? GPIO_PIN_1 : GPIO_PIN_4;
            g_keys[i]->event_out = KEY_EVENT_NONE;
            g_keys[i]->is_physically_low = false;
            
            /* 初始化状态机上下文 */
            smf_set_initial(SMF_CTX(g_keys[i]), &key_fsm[S_IDLE]);
            
            /* 注册到 BareTask 调度器 */
            bare_task_create(KEY_Scan_Task, g_keys[i], SCAN_PERIOD_MS);
        }
    }
}

void KEY_Scan_Task(void* arg) {
    KEY_Obj_t *k = (KEY_Obj_t *)arg;
    if (!k) return;

    // 1. 同步物理电平
    k->is_physically_low = (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_RESET);

    // 2. 运行状态机迭代
    smf_run_state(SMF_CTX(k));

    // 3. 检查并打印输出事件
    if (k->event_out != KEY_EVENT_NONE) {
        const char* key_name = (k->pin == GPIO_PIN_1) ? "KEY2" : "KEY3";
        
        switch (k->event_out) {
            case KEY_EVENT_PRESSED:
                LOG_INFO("%s: SINGLE CLICK", key_name);
                break;
            case KEY_EVENT_DOUBLE_CLICK:
                LOG_INFO("%s: DOUBLE CLICK", key_name);
                break;
            case KEY_EVENT_LONG_PRESSED:
                LOG_INFO("%s: LONG PRESS", key_name);
                break;
            default:
                break;
        }
        k->event_out = KEY_EVENT_NONE; 
    }
}

KEY_Event_t KEY_GetEvent(KEY_ID_t key_id) {
    if (key_id >= KEY_MAX_NUM || g_keys[key_id] == NULL) return KEY_EVENT_NONE;
    
    KEY_Event_t ev = g_keys[key_id]->event_out;
    g_keys[key_id]->event_out = KEY_EVENT_NONE; // Clear on read
    return ev;
}