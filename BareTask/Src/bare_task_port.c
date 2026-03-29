#include "main.h"
#include <stdbool.h>



int32_t bare_task_get_ms(void)
{
    return HAL_GetTick();
}
static uint32_t cpu_sr;
/* bare task lock */
void bare_task_lock(void)
{
    cpu_sr = __get_PRIMASK();
    __disable_irq();
}
/* bare task unlock */
void bare_task_unlock(void)
{
    __set_PRIMASK(cpu_sr);
}
// 是否处于中断中
bool bare_task_in_interrupt(void)
{
    return (__get_IPSR() != 0); 
}

