#include "bare_task.h"
#include "heap.h"
#include <stdbool.h>
#include <stdint.h>


/* Defines the maximum number of tasks the system can manage */
#define BARE_TASK_MAX_COUNT    16
/* External time interface provided by the platform port */
extern int32_t bare_task_get_ms(void);
/* Internal task list */
static struct bare_task_t* g_task_list[BARE_TASK_MAX_COUNT];
static uint32_t g_task_id_counter = 0;

struct bare_task_t {
    BareTaskFunc_t task_func;
    void* task_arg;
    uint8_t task_state;
    uint32_t task_interval;
    uint32_t task_last_run;
    uint32_t task_id;
    bool task_enable;
};


/**
 * @brief Initialize the BareTask module.
 */
void bare_task_init(void) {
    for (int i = 0; i < BARE_TASK_MAX_COUNT; i++) {
        g_task_list[i] = NULL;
    }
    g_task_id_counter = 0;
}



/**
 * @brief Create and register a new task using dynamic memory (Heap4).
 * * @param task_func Pointer to the function to be executed.
 * @param task_arg  Pointer to user arguments.
 * @param task_interval Execution period in milliseconds.
 * @return struct bare_task_t* Pointer to the created task, or NULL if failed.
 */
struct bare_task_t* bare_task_create(BareTaskFunc_t task_func, void* task_arg, uint32_t task_interval) {
    if (task_func == NULL) return NULL;

    /* Find an empty slot in the task list */
    int slot = -1;
    for (int i = 0; i < BARE_TASK_MAX_COUNT; i++) {
        if (g_task_list[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return NULL; // No free slots

    /* Allocate TCB from the Heap4 memory pool */
    struct bare_task_t* new_task = (struct bare_task_t*)pvPortMalloc(sizeof(struct bare_task_t));
    
    if (new_task != NULL) {
        new_task->task_func     = task_func;
        new_task->task_arg      = task_arg;
        new_task->task_interval = (task_interval < 0) ? 0 : (uint32_t)task_interval;
        new_task->task_last_run = bare_task_get_ms();
        new_task->task_state    = 0; // Default state
        new_task->task_id       = g_task_id_counter++;
        new_task->task_enable   = true;

        g_task_list[slot] = new_task;
    }

    return new_task;
}

/**
 * @brief The core scheduler loop. Call this in the main while(1).
 */
void bare_task_run(void) {
    uint32_t current_tick = bare_task_get_ms();

    for (int i = 0; i < BARE_TASK_MAX_COUNT; i++) {
        struct bare_task_t* t = g_task_list[i];

        /* Check if task exists and is enabled */
        if (t != NULL && t->task_enable) {
            
            /* Check if the interval has passed (Handles wrap-around) */
            if (current_tick - t->task_last_run >= t->task_interval) {
                
                /* Update last run time before execution to prevent drift in long tasks */
                t->task_last_run = current_tick;

                /* Execute the task function */
                t->task_func(t->task_arg);
            }
        }
    }
}