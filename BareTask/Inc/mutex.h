#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>
#include <stdbool.h>

/* 使用不透明指针隐藏实现 */
struct mutex;
struct recursive_mutex;

typedef struct mutex mutex_t;
typedef struct recursive_mutex recursive_mutex_t;

/* --- 基础互斥锁接口 --- */
mutex_t* mutex_create(void);
void mutex_delete(mutex_t* mutex);

/**
 * @brief 获取互斥锁
 * @param task_id 当前任务的唯一ID
 * @return true 成功获取, false 失败（锁被占用或处于中断中）
 */
bool mutex_lock(mutex_t* mutex, uint32_t task_id);
void mutex_unlock(mutex_t* mutex, uint32_t task_id);

/* --- 递归互斥锁接口 --- */
recursive_mutex_t* recursive_mutex_create(void);
void recursive_mutex_delete(recursive_mutex_t* r_mutex);

/**
 * @brief 获取递归互斥锁
 * @param task_id 当前任务的唯一ID
 */
bool recursive_mutex_lock(recursive_mutex_t* r_mutex, uint32_t task_id);
void recursive_mutex_unlock(recursive_mutex_t* r_mutex, uint32_t task_id);

#endif