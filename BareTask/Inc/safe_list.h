#ifndef SAFE_LIST_H
#define SAFE_LIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 链表节点结构 */
struct safe_list_node {
    struct safe_list_node *next;
    struct safe_list_node *prev;
};

/* 链表头结构 */
struct safe_list_head {
    struct safe_list_node head;
    uint32_t count;
};

/**
 * @brief 常用宏：通过成员指针获取结构体起始地址
 */
#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* 初始化链表头 */
void safe_list_init(struct safe_list_head *list);

/* 向链表尾部添加节点（动态分配内存） */
bool safe_list_add_tail(struct safe_list_head *list, void *data, size_t data_size);

/* 从链表中移除特定节点并释放内存 */
void safe_list_remove_and_free(struct safe_list_head *list, struct safe_list_node *node);

/* 线程安全的遍历接口示例：获取下一个节点 */
struct safe_list_node* safe_list_get_next(struct safe_list_head *list, struct safe_list_node *current);

#ifdef __cplusplus
}
#endif

#endif /* SAFE_LIST_H */