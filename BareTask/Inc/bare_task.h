#ifndef BARE_TASK_H
#define BARE_TASK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*BareTaskFunc_t)(void *pvParameters);
struct bare_task_t;

void bare_task_init(void);
/* 创建任务：现在不再受最大数量限制 */
struct bare_task_t* bare_task_create(BareTaskFunc_t task_func, void* task_arg, uint32_t task_interval);
/* 销毁任务：释放内存并从链表中移除 */
void bare_task_delete(struct bare_task_t* task);
/* 启用/禁用任务 */
void bare_task_set_enable(struct bare_task_t* task, bool enable);
void bare_task_run(void);

#ifdef __cplusplus
}
#endif

#endif