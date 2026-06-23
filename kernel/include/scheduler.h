/* SPDX-License-Identifier: Proprietary */
#pragma once
/* scheduler.h — task scheduler interface */

#include "types.h"

#define MAX_TASKS       256
#define TASK_NAME_MAX   32

typedef enum {
    TASK_READY   = 0,
    TASK_RUNNING = 1,
    TASK_BLOCKED = 2,
    TASK_DEAD    = 3,
} task_state_t;

typedef struct task_context {
    u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    u64 fp;   /* x29 */
    u64 lr;   /* x30 */
    u64 sp;
} task_context_t;

typedef struct task {
    u32           tid;
    task_state_t  state;
    task_context_t ctx;
    void         *stack_base;
    u64           stack_size;
    u64           priority;
    char          name[TASK_NAME_MAX];
    struct task  *next;
} task_t;

void    scheduler_init(void);
task_t *task_create(const char *name, void (*entry)(void), u64 priority);
void    task_destroy(task_t *task);
void    scheduler_run(void);
void    scheduler_yield(void);
task_t *scheduler_current(void);
u32     scheduler_task_count(void);
u32     scheduler_ready_count(void);
int     scheduler_bringup_selftest(void);
int     scheduler_first_init_thread_selftest(void);
