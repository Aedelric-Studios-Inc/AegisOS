/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/thread.c
 * Kernel thread support — lightweight threads sharing process address space.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"

#define THREAD_STACK_SIZE  (16 * 1024)  /* 16 KB kernel thread stack */
#define MAX_THREADS        512

typedef struct kthread {
    u32     tid;
    task_t *task;
    u32     owner_pid;
    bool    active;
    bool    detached;
    void   *retval;
} kthread_t;

static kthread_t thread_table[MAX_THREADS];
static u32 next_thread_id = 1;

void kthread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].active = false;
    }
}

int kthread_create(const char *name, void (*entry)(void), u32 owner_pid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!thread_table[i].active) {
            task_t *t = task_create(name, entry, 0);
            if (!t) return AEGIS_ENOMEM;

            thread_table[i].tid = next_thread_id++;
            thread_table[i].task = t;
            thread_table[i].owner_pid = owner_pid;
            thread_table[i].active = true;
            thread_table[i].detached = false;
            thread_table[i].retval = NULL;
            return (int)thread_table[i].tid;
        }
    }
    return AEGIS_ENOMEM;
}

void kthread_exit(void *retval) {
    task_t *current = scheduler_current();
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].active && thread_table[i].task == current) {
            thread_table[i].retval = retval;
            thread_table[i].active = false;
            task_destroy(current);
            scheduler_yield();
            return;
        }
    }
}

int kthread_join(u32 tid, void **retval) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].tid == tid) {
            if (thread_table[i].active) {
                /* Thread still running — busy wait (simplified) */
                while (thread_table[i].active) {
                    scheduler_yield();
                }
            }
            if (retval) *retval = thread_table[i].retval;
            thread_table[i].active = false;
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}

int kthread_detach(u32 tid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].tid == tid && thread_table[i].active) {
            thread_table[i].detached = true;
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}
