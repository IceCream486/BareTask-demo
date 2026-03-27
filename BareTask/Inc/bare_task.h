#ifndef BARE_TASK_H
#define BARE_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* task function type */
typedef void (*BareTaskFunc_t)(void *pvParameters);
/* task structure */
struct bare_task_t;

/* init bare_task module */
void bare_task_init(void);
/* create a new task */
struct bare_task_t* bare_task_create(BareTaskFunc_t task_func, void* task_arg, uint32_t task_interval);
/* bare task loop */
void bare_task_run(void);



#ifdef __cplusplus
}
#endif

#endif // BARE_TASK_H