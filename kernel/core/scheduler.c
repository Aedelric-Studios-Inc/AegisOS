/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/scheduler.c
 * Simple round-robin scheduler.
 */

#include "scheduler.h"
#include "memory.h"
#include "panic.h"

static task_t  *task_list_head = NULL;
static task_t  *current_task   = NULL;
static u32      next_tid       = 1;

/* Defined in boot/arm64/context_switch.S */
extern void context_switch(task_context_t *prev, task_context_t *next);

void scheduler_init(void) {
    task_list_head = NULL;
    current_task   = NULL;
}

task_t *task_create(const char *name, void (*entry)(void), u64 priority) {
    task_t *t = (task_t *)kmalloc(sizeof(task_t));
    if (!t) return NULL;

    t->tid      = next_tid++;
    t->state    = TASK_READY;
    t->priority = priority;
    t->next     = NULL;

    /* Copy name safely */
    int i = 0;
    while (name[i] && i < TASK_NAME_MAX - 1) { t->name[i] = name[i]; i++; }
    t->name[i] = '\0';

    /* Set up initial context — LR points to entry function */
    __builtin_memset(&t->ctx, 0, sizeof(t->ctx));
    t->ctx.lr = (u64)entry;

    /* Allocate a stack */
    void *stack = kmalloc(8192);
    if (!stack) { kfree(t); return NULL; }
    t->ctx.sp = (u64)stack + 8192;

    /* Append to list */
    if (!task_list_head) {
        task_list_head = t;
    } else {
        task_t *p = task_list_head;
        while (p->next) p = p->next;
        p->next = t;
    }

    return t;
}

void task_destroy(task_t *task) {
    if (!task) return;
    task->state = TASK_DEAD;
}

void scheduler_run(void) {
    if (!task_list_head) PANIC("no tasks to schedule");
    current_task = task_list_head;
    current_task->state = TASK_RUNNING;
    /* Jump into first task — no previous context to save */
    task_context_t dummy = {0};
    context_switch(&dummy, &current_task->ctx);
}

void scheduler_yield(void) {
    task_t *prev = current_task;
    task_t *next = prev->next ? prev->next : task_list_head;
    /* Skip dead tasks */
    while (next != prev && next->state == TASK_DEAD) {
        next = next->next ? next->next : task_list_head;
    }
    if (next == prev) return;
    prev->state  = TASK_READY;
    next->state  = TASK_RUNNING;
    current_task = next;
    context_switch(&prev->ctx, &next->ctx);
}

task_t *scheduler_current(void) {
    return current_task;
}
