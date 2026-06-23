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

static void task_name_copy(char *dst, const char *src) {
    u32 i = 0;
    if (!src) src = "task";
    while (src[i] && i < TASK_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void task_append(task_t *t) {
    if (!task_list_head) {
        task_list_head = t;
        return;
    }
    task_t *p = task_list_head;
    while (p->next) p = p->next;
    p->next = t;
}

static task_t *scheduler_pick_first_ready(void) {
    task_t *best = NULL;
    for (task_t *p = task_list_head; p; p = p->next) {
        if (p->state != TASK_READY) continue;
        if (!best || p->priority > best->priority) {
            best = p;
        }
    }
    return best;
}

void scheduler_init(void) {
    task_list_head = NULL;
    current_task   = NULL;
    next_tid       = 1;
}

task_t *task_create(const char *name, void (*entry)(void), u64 priority) {
    if (!entry) return NULL;

    task_t *t = (task_t *)kmalloc(sizeof(task_t));
    if (!t) return NULL;

    __builtin_memset(t, 0, sizeof(*t));
    t->tid        = next_tid++;
    t->state      = TASK_READY;
    t->priority   = priority;
    t->stack_size = 8192;
    task_name_copy(t->name, name);

    void *stack = kmalloc(t->stack_size);
    if (!stack) {
        kfree(t);
        return NULL;
    }

    t->stack_base = stack;
    t->ctx.sp = (u64)stack + t->stack_size;
    t->ctx.lr = (u64)entry;

    task_append(t);
    return t;
}

void task_destroy(task_t *task) {
    if (!task) return;

    if (current_task == task) {
        task->state = TASK_DEAD;
        return;
    }

    task_t *prev = NULL;
    task_t *p = task_list_head;
    while (p && p != task) {
        prev = p;
        p = p->next;
    }
    if (!p) return;

    if (prev) prev->next = p->next;
    else task_list_head = p->next;

    p->state = TASK_DEAD;
    if (p->stack_base) {
        kfree(p->stack_base);
        p->stack_base = NULL;
    }
    kfree(p);
}

void scheduler_run(void) {
    task_t *first = scheduler_pick_first_ready();
    if (!first) PANIC("no ready tasks to schedule");

    current_task = first;
    current_task->state = TASK_RUNNING;

    /* Jump into first task — no previous task context exists yet. */
    task_context_t dummy = {0};
    context_switch(&dummy, &current_task->ctx);

    PANIC("initial context switch returned");
}

void scheduler_yield(void) {
    if (!current_task || !task_list_head) return;

    task_t *prev = current_task;
    task_t *next = prev->next ? prev->next : task_list_head;

    while (next != prev && next->state != TASK_READY) {
        next = next->next ? next->next : task_list_head;
    }
    if (next == prev || next->state != TASK_READY) return;

    prev->state  = TASK_READY;
    next->state  = TASK_RUNNING;
    current_task = next;
    context_switch(&prev->ctx, &next->ctx);
}

task_t *scheduler_current(void) {
    return current_task;
}

u32 scheduler_task_count(void) {
    u32 count = 0;
    for (task_t *p = task_list_head; p; p = p->next) {
        count++;
    }
    return count;
}

u32 scheduler_ready_count(void) {
    u32 count = 0;
    for (task_t *p = task_list_head; p; p = p->next) {
        if (p->state == TASK_READY) count++;
    }
    return count;
}

static void scheduler_selftest_task_a(void) {
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

static void scheduler_selftest_task_b(void) {
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

static void scheduler_first_init_selftest_entry(void) {
    for (;;) {
        __asm__ volatile("wfe" ::: "memory");
    }
}

int scheduler_bringup_selftest(void) {
    if (scheduler_current() != NULL) return AEGIS_EBUSY;
    u32 before = scheduler_task_count();

    task_t *a = task_create("sched-proof-a", scheduler_selftest_task_a, 1);
    task_t *b = task_create("sched-proof-b", scheduler_selftest_task_b, 1);
    if (!a || !b) return AEGIS_ENOMEM;
    if (a == b) return AEGIS_EINVAL;
    if (a->tid == b->tid || a->tid == 0 || b->tid == 0) return AEGIS_EINVAL;
    if (a->state != TASK_READY || b->state != TASK_READY) return AEGIS_EINVAL;
    if (a->ctx.sp == 0 || b->ctx.sp == 0 || a->ctx.sp == b->ctx.sp) return AEGIS_EINVAL;
    if (!a->stack_base || !b->stack_base || a->stack_base == b->stack_base) return AEGIS_EINVAL;
    if (a->ctx.lr != (u64)scheduler_selftest_task_a) return AEGIS_EINVAL;
    if (b->ctx.lr != (u64)scheduler_selftest_task_b) return AEGIS_EINVAL;

    u32 after = scheduler_task_count();
    if (after != before + 2U) return AEGIS_EINVAL;
    if (scheduler_ready_count() < 2U) return AEGIS_EINVAL;

    task_destroy(b);
    task_destroy(a);
    if (scheduler_task_count() != before) return AEGIS_EINVAL;

    return AEGIS_OK;
}

int scheduler_first_init_thread_selftest(void) {
    if (scheduler_current() != NULL) return AEGIS_EBUSY;

    u32 before = scheduler_task_count();
    task_t *init = task_create("aegisbox-init-proof", scheduler_first_init_selftest_entry, 100);
    if (!init) return AEGIS_ENOMEM;
    if (init->state != TASK_READY) return AEGIS_EINVAL;
    if (init->priority != 100U) return AEGIS_EINVAL;
    if (init->ctx.lr != (u64)scheduler_first_init_selftest_entry) return AEGIS_EINVAL;
    if (init->ctx.sp == 0 || !init->stack_base || init->stack_size < 4096U) return AEGIS_EINVAL;
    if (scheduler_pick_first_ready() != init) return AEGIS_EINVAL;
    if (scheduler_task_count() != before + 1U) return AEGIS_EINVAL;

    task_destroy(init);
    if (scheduler_task_count() != before) return AEGIS_EINVAL;
    return AEGIS_OK;
}
