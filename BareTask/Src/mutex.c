#include "mutex.h"
#include "heap.h"
#include <stddef.h>

/* 引用外部底层接口 */
extern void bare_task_lock(void);    /* 全局硬件锁（如关中断） */
extern void bare_task_unlock(void);  /* 全局硬件解锁 */
extern bool bare_task_in_interrupt(void); /* 判断当前是否在中断上下文中 */

/* 内部结构体定义 */
struct mutex {
    uint32_t owner;    /* 持有锁的任务ID */
    bool is_locked;    /* 锁定状态标记 */
};

struct recursive_mutex {
    uint32_t owner;      /* 持有锁的任务ID */
    uint32_t lock_count; /* 递归计数器 */
};

// ---------------- 基础互斥锁实现 ----------------

mutex_t* mutex_create(void) {
    mutex_t* m = (mutex_t*)pvPortMalloc(sizeof(struct mutex));
    if (m) {
        m->owner = 0;
        m->is_locked = false;
    }
    return m;
}

void mutex_delete(mutex_t* mutex) {
    if (mutex) {
        vPortFree(mutex);
        mutex = NULL;
    }
}

bool mutex_lock(mutex_t* mutex, uint32_t task_id) {
    /* 安全检查：1. 指针有效性；2. 中断中禁止使用互斥锁 */
    if (!mutex || bare_task_in_interrupt()) {
        return false;
    }
    
    bool success = false;
    bare_task_lock(); /* 进入临界区，保护锁状态原子性 */

    if (!mutex->is_locked) {
        mutex->owner = task_id;
        mutex->is_locked = true;
        success = true;
    }

    bare_task_unlock();
    return success;
}

void mutex_unlock(mutex_t* mutex, uint32_t task_id) {
    /* 中断中禁止操作任务级互斥锁 */
    if (!mutex || bare_task_in_interrupt()) {
        return;
    }

    bare_task_lock();
    /* 安全检查：只有持有锁的任务 ID 匹配时才能解锁 */
    if (mutex->is_locked && mutex->owner == task_id) {
        mutex->is_locked = false;
        mutex->owner = 0;
    }
    bare_task_unlock();
}

// ---------------- 递归互斥锁实现 ----------------

recursive_mutex_t* recursive_mutex_create(void) {
    recursive_mutex_t* rm = (recursive_mutex_t*)pvPortMalloc(sizeof(struct recursive_mutex));
    if (rm) {
        rm->owner = 0;
        rm->lock_count = 0;
    }
    return rm;
}

void recursive_mutex_delete(recursive_mutex_t* r_mutex) {
    if (r_mutex) {
        vPortFree(r_mutex);
        r_mutex = NULL;
    }
}

bool recursive_mutex_lock(recursive_mutex_t* r_mutex, uint32_t task_id) {
    if (!r_mutex || bare_task_in_interrupt()) {
        return false;
    }

    bool success = false;
    bare_task_lock();

    /* 情况1：锁未被占用（计数为0） */
    if (r_mutex->lock_count == 0) {
        r_mutex->owner = task_id;
        r_mutex->lock_count = 1;
        success = true;
    }
    /* 情况2：当前任务已经是持有者 (允许递归重入) */
    else if (r_mutex->owner == task_id) {
        r_mutex->lock_count++;
        success = true;
    }

    bare_task_unlock();
    return success;
}

void recursive_mutex_unlock(recursive_mutex_t* r_mutex, uint32_t task_id) {
    if (!r_mutex || bare_task_in_interrupt()) {
        return;
    }

    bare_task_lock();
    
    /* 确认解锁者身份并确保当前有锁 */
    if (r_mutex->owner == task_id && r_mutex->lock_count > 0) {
        r_mutex->lock_count--;
        /* 只有当递归计数降为 0 时，才清除所有权 */
        if (r_mutex->lock_count == 0) {
            r_mutex->owner = 0;
        }
    }
    
    bare_task_unlock();
}