#include "safe_list.h"
#include "heap.h"
#include <string.h>
#include "mutex.h"

/* 外部提供的锁接口，需与 heap.c 中的 HEAP_LOCK 保持一致 */
extern void bare_task_lock(void);
extern void bare_task_unlock(void);

/**
 * @brief 内部封装的节点，包含用户数据
 */
struct safe_list_item {
    struct safe_list_node node;
    uint8_t data[]; // 柔性数组，存储实际资源内容
};

void safe_list_init(struct safe_list_head *list) {
    if (list == NULL) return;
    
    bare_task_lock();
    list->head.next = &list->head;
    list->head.prev = &list->head;
    list->count = 0;
    bare_task_unlock();
}

bool safe_list_add_tail(struct safe_list_head *list, void *data, size_t data_size) {
    if (list == NULL || data == NULL) return false;

    // 1. 先在锁外申请内存，减少锁持有时间
    struct safe_list_item *new_item = (struct safe_list_item *)pvPortMalloc(sizeof(struct safe_list_item) + data_size);
    if (new_item == NULL) return false;

    memcpy(new_item->data, data, data_size);

    // 2. 进入临界区修改指针
    bare_task_lock();
    struct safe_list_node *head = &list->head;
    struct safe_list_node *new_node = &new_item->node;

    new_node->next = head;
    new_node->prev = head->prev;
    head->prev->next = new_node;
    head->prev = new_node;
    
    list->count++;
    bare_task_unlock();

    return true;
}

void safe_list_remove_and_free(struct safe_list_head *list, struct safe_list_node *node) {
    if (list == NULL || node == NULL || node == &list->head) return;

    bare_task_lock();
    // 断开连接
    node->prev->next = node->next;
    node->next->prev = node->prev;
    list->count--;
    bare_task_unlock();

    // 在锁外释放内存
    struct safe_list_item *item = container_of(node, struct safe_list_item, node);
    vPortFree(item);
}

struct safe_list_node* safe_list_get_next(struct safe_list_head *list, struct safe_list_node *current) {
    struct safe_list_node *next = NULL;
    
    bare_task_lock();
    if (list->count > 0) {
        if (current == NULL || current == &list->head) {
            next = list->head.next;
        } else {
            next = current->next;
            if (next == &list->head) next = NULL; // 遍历结束
        }
    }
    bare_task_unlock();
    
    return next;
}