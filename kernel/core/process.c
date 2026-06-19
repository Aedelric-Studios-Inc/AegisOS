/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/process.c
 * Process lifecycle management — creation, destruction, signals,
 * wait/exit semantics for user-space processes.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "panic.h"

#define MAX_PROCESSES  256
#define PROC_STACK_SIZE  (64 * 1024)  /* 64 KB user stack */

typedef enum {
    PROC_STATE_EMPTY = 0,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING,
    PROC_STATE_ZOMBIE,
    PROC_STATE_STOPPED,
} proc_state_t;

typedef struct process {
    u32          pid;
    u32          ppid;         /* Parent PID */
    proc_state_t state;
    int          exit_code;
    u64          start_time;
    u64          cpu_time;
    virt_addr_t  stack_base;
    u64          stack_size;
    task_t      *task;         /* Associated scheduler task */
    cap_token_t  cap_set[16]; /* Capabilities held by this process */
    u32          cap_count;
    char         name[TASK_NAME_MAX];
    u32          uid;
    u32          gid;
} process_t;

static process_t proc_table[MAX_PROCESSES];
static u32 next_pid = 1;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        proc_table[i].state = PROC_STATE_EMPTY;
        proc_table[i].pid = 0;
    }
    next_pid = 1;
}

static process_t *proc_find_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY)
            return &proc_table[i];
    }
    return NULL;
}

process_t *process_create(const char *name, void (*entry)(void), u32 ppid) {
    process_t *proc = proc_find_slot();
    if (!proc) return NULL;

    proc->pid = next_pid++;
    proc->ppid = ppid;
    proc->state = PROC_STATE_RUNNING;
    proc->exit_code = 0;
    proc->start_time = kernel_get_ticks();
    proc->cpu_time = 0;
    proc->cap_count = 0;
    proc->uid = 0;
    proc->gid = 0;

    /* Copy name */
    int i = 0;
    while (name[i] && i < TASK_NAME_MAX - 1) { proc->name[i] = name[i]; i++; }
    proc->name[i] = '\0';

    /* Allocate stack */
    void *stack = kmalloc(PROC_STACK_SIZE);
    if (!stack) {
        proc->state = PROC_STATE_EMPTY;
        return NULL;
    }
    proc->stack_base = (virt_addr_t)stack;
    proc->stack_size = PROC_STACK_SIZE;

    /* Create scheduler task for this process */
    proc->task = task_create(name, entry, 1);
    if (!proc->task) {
        kfree(stack);
        proc->state = PROC_STATE_EMPTY;
        return NULL;
    }

    return proc;
}

int process_kill(u32 pid, int signal) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_STATE_EMPTY) {
            if (signal == 9) { /* SIGKILL */
                proc_table[i].state = PROC_STATE_ZOMBIE;
                proc_table[i].exit_code = -signal;
                if (proc_table[i].task) {
                    task_destroy(proc_table[i].task);
                }
            } else if (signal == 19) { /* SIGSTOP */
                proc_table[i].state = PROC_STATE_STOPPED;
            } else if (signal == 18) { /* SIGCONT */
                if (proc_table[i].state == PROC_STATE_STOPPED)
                    proc_table[i].state = PROC_STATE_RUNNING;
            }
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}

void process_exit(int code) {
    task_t *current = scheduler_current();
    if (!current) return;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].task == current) {
            proc_table[i].state = PROC_STATE_ZOMBIE;
            proc_table[i].exit_code = code;
            task_destroy(current);

            /* Reparent children to init (PID 1) */
            u32 my_pid = proc_table[i].pid;
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (proc_table[j].ppid == my_pid &&
                    proc_table[j].state != PROC_STATE_EMPTY) {
                    proc_table[j].ppid = 1;
                }
            }
            break;
        }
    }
    scheduler_yield();
}

int process_wait(u32 pid, int *exit_code) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid) {
            if (proc_table[i].state == PROC_STATE_ZOMBIE) {
                if (exit_code) *exit_code = proc_table[i].exit_code;
                /* Reap the zombie */
                proc_table[i].state = PROC_STATE_EMPTY;
                if (proc_table[i].stack_base) {
                    kfree((void *)proc_table[i].stack_base);
                }
                return AEGIS_OK;
            }
            return AEGIS_EBUSY; /* Still running */
        }
    }
    return AEGIS_ENOENT;
}

process_t *process_get(u32 pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_STATE_EMPTY)
            return &proc_table[i];
    }
    return NULL;
}

process_t *process_current(void) {
    task_t *current = scheduler_current();
    if (!current) return NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].task == current)
            return &proc_table[i];
    }
    return NULL;
}

u32 process_getpid(void) {
    process_t *p = process_current();
    return p ? p->pid : 0;
}

int process_grant_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return AEGIS_ENOENT;
    if (p->cap_count >= 16) return AEGIS_ENOMEM;
    p->cap_set[p->cap_count++] = cap;
    return AEGIS_OK;
}

int process_revoke_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return AEGIS_ENOENT;
    for (u32 i = 0; i < p->cap_count; i++) {
        if (p->cap_set[i] == cap) {
            /* Compact the array */
            p->cap_set[i] = p->cap_set[--p->cap_count];
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}

bool process_has_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return false;
    for (u32 i = 0; i < p->cap_count; i++) {
        if (p->cap_set[i] == cap) return true;
    }
    return false;
}
