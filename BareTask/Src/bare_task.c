#include "bare_task.h"
#include "heap.h"
#include "safe_list.h" // 使用之前定义的双向链表
#include "log.h"
#include "mutex.h"

/* 外部时间接口 */
extern int32_t bare_task_get_ms(void);
/* 全局任务管理链表头 */
static struct safe_list_head g_task_dynamic_list;
static uint32_t g_task_id_counter = 1;
static mutex_t* g_task_list_mutex; // 保护任务链表的互斥锁

struct bare_task_t {
    struct safe_list_node node; // 包含链表指针
    BareTaskFunc_t task_func;
    void* task_arg;
    uint32_t task_interval;
    uint32_t task_last_run;
    uint32_t task_id;
    bool task_enable;
};

void bare_task_init(void) {
    safe_list_init(&g_task_dynamic_list); // 初始化安全链表
    g_task_id_counter = 1;
    g_task_list_mutex = mutex_create();
    LOG_DEBUG("bare task dynamic init ok");
}

struct bare_task_t* bare_task_create(BareTaskFunc_t task_func, void* task_arg, uint32_t task_interval) {
    if (task_func == NULL) return NULL;

    /* 动态申请任务控制块内存 */
    struct bare_task_t* new_task = (struct bare_task_t*)pvPortMalloc(sizeof(struct bare_task_t));
    if (new_task == NULL) {
        LOG_WARN("malloc failed for new task");
        return NULL;
    }

    new_task->task_func     = task_func;
    new_task->task_arg      = task_arg;
    new_task->task_interval = task_interval;
    new_task->task_last_run = (uint32_t)bare_task_get_ms();
    new_task->task_enable   = true;

    mutex_lock(g_task_list_mutex, 0);
    new_task->task_id = g_task_id_counter++;
    
    /* 将节点插入链表末尾 */
    struct safe_list_node *head = &g_task_dynamic_list.head;
    new_task->node.next = head;
    new_task->node.prev = head->prev;
    head->prev->next = &new_task->node;
    head->prev = &new_task->node;
    g_task_dynamic_list.count++;
    mutex_unlock(g_task_list_mutex, 0);

    LOG_DEBUG("new task created, id: %d", new_task->task_id);
    return new_task;
}

void bare_task_delete(struct bare_task_t* task) {
    if (task == NULL) return;

    mutex_lock(g_task_list_mutex, 0);
    /* 从双向链表中脱离 */
    task->node.prev->next = task->node.next;
    task->node.next->prev = task->node.prev;
    mutex_unlock(g_task_list_mutex, 0);


    LOG_DEBUG("delete task id: %d", task->task_id);
    vPortFree(task); // 释放内存
}

void bare_task_set_enable(struct bare_task_t* task, bool enable) {
    if (task != NULL) {
        mutex_lock(g_task_list_mutex, 0);
        task->task_enable = enable;
        mutex_unlock(g_task_list_mutex, 0);
    }
}

void bare_task_run(void) {
    uint32_t current_tick = (uint32_t)bare_task_get_ms();
    struct safe_list_node *pos;

    /* 遍历链表执行任务 */
    // 注意：为了安全，我们在访问链表指针时上锁
    for (pos = g_task_dynamic_list.head.next; pos != &g_task_dynamic_list.head; ) {
        
        mutex_lock(g_task_list_mutex, 0);
        struct bare_task_t* t = container_of(pos, struct bare_task_t, node);
        struct safe_list_node *next_pos = pos->next; // 预先获取下一个，防止执行中发生删除
        mutex_unlock(g_task_list_mutex, 0);

        if (t->task_enable) {
            if (current_tick - t->task_last_run >= t->task_interval) {
                t->task_last_run = current_tick;
                /* 执行任务函数 */
                t->task_func(t->task_arg);
            }
        }
        pos = next_pos;
    }
}