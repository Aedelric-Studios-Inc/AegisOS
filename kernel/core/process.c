/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/process.c
 * Native process lifecycle management.
 *
 * v57 adds distinct user image/stack metadata and a kernel task trampoline for
 * each EL0 process. Cooperative SYS_YIELD switches between the saved syscall
 * frames on each task's kernel stack; timer-IRQ preemption remains gated until
 * exception-frame-aware preemption is implemented.
 */

#include "types.h"
#include "memory.h"
#include "scheduler.h"
#include "panic.h"
#include "kernel_timer.h"
#include "process.h"
#include "elf_loader.h"
#include "userland.h"
#include "service_supervisor.h"
#include "fd_table.h"
#include "socket.h"

#define MAX_PROCESSES   256
#define PROC_STACK_SIZE (64 * 1024)

typedef enum {
    PROC_STATE_EMPTY = 0,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING,
    PROC_STATE_ZOMBIE,
    PROC_STATE_STOPPED,
} proc_state_t;

struct process {
    u32          pid;
    u32          ppid;
    proc_state_t state;
    int          exit_code;
    u64          start_time;
    u64          cpu_time;
    virt_addr_t  entry;
    virt_addr_t  text_base;
    u64          text_size;
    virt_addr_t  stack_base;
    u64          stack_size;
    u32          runtime_slot;
    bool         owns_stack;
    task_t      *task;
    cap_token_t  cap_set[16];
    u32          cap_count;
    char         name[TASK_NAME_MAX];
    u32          uid;
    u32          gid;
};

static process_t proc_table[MAX_PROCESSES];
static u32 next_pid = 1;
static bool proc_table_cleanup_ready;

extern void aegis_el0_transition_enter(u64 entry, u64 user_sp);
extern void printk(const char *fmt, ...);

static void process_zero(process_t *proc) {
    if (!proc) return;
    u8 *bytes = (u8 *)proc;
    for (u64 i = 0; i < sizeof(*proc); i++) bytes[i] = 0;
    proc->state = PROC_STATE_EMPTY;
}

static void process_copy_name(char *dst, const char *src) {
    u32 i = 0;
    if (!src) src = "process";
    while (src[i] && i < TASK_NAME_MAX - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool range_inside(u64 ptr, u64 len, u64 base, u64 size) {
    if (!ptr || !len || !base || !size) return false;
    u64 end = ptr + len;
    u64 region_end = base + size;
    if (end <= ptr || region_end <= base) return false;
    return ptr >= base && end <= region_end;
}

void process_init(void) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) process_zero(&proc_table[i]);
    next_pid = 1;
    proc_table_cleanup_ready = false;
    fd_table_init();
}

static process_t *proc_find_slot(void) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY) return &proc_table[i];
    }
    return NULL;
}

static void process_initialise_record(process_t *proc, const char *name, u32 ppid) {
    process_zero(proc);
    proc->pid = next_pid++;
    proc->ppid = ppid;
    proc->state = PROC_STATE_RUNNING;
    proc->start_time = kernel_get_ticks();
    process_copy_name(proc->name, name);
}

static void process_user_task_entry(void) {
    process_t *proc = process_current();
    if (!proc || !proc->entry || !proc->stack_base || !proc->stack_size) {
        PANIC("native user task missing process runtime");
    }

    u64 stack_top = (proc->stack_base + proc->stack_size) & ~0xFULL;
    printk("[AegisOS:native] entering %s pid=%u ppid=%u entry=%p stack=%p\n",
           proc->name,
           proc->pid,
           proc->ppid,
           (void *)(uptr)proc->entry,
           (void *)(uptr)stack_top);
    aegis_el0_transition_enter(proc->entry, stack_top);
    PANIC("native EL0 process returned without SYS_EXIT");
}

process_t *process_create(const char *name, void (*entry)(void), u32 ppid) {
    if (!name || !entry) return NULL;
    process_t *proc = proc_find_slot();
    if (!proc) return NULL;
    process_initialise_record(proc, name, ppid);

    void *stack = kmalloc(PROC_STACK_SIZE);
    if (!stack) {
        process_zero(proc);
        return NULL;
    }
    proc->stack_base = (virt_addr_t)stack;
    proc->stack_size = PROC_STACK_SIZE;
    proc->owns_stack = true;

    proc->task = task_create(name, entry, 1);
    if (!proc->task) {
        kfree(stack);
        process_zero(proc);
        return NULL;
    }
    return proc;
}

process_t *process_bind_current_user_image(const char *name,
                                           virt_addr_t entry,
                                           virt_addr_t text_base,
                                           u64 text_size,
                                           virt_addr_t user_stack_base,
                                           u64 user_stack_size,
                                           u32 runtime_slot,
                                           u32 ppid) {
    task_t *current = scheduler_current();
    if (!current || !name || !entry || !text_base || !text_size ||
        !user_stack_base || !user_stack_size || !runtime_slot) return NULL;

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_STATE_EMPTY && proc_table[i].task == current) return NULL;
    }

    process_t *proc = proc_find_slot();
    if (!proc) return NULL;
    process_initialise_record(proc, name, ppid);
    proc->entry = entry;
    proc->text_base = text_base;
    proc->text_size = text_size;
    proc->stack_base = user_stack_base;
    proc->stack_size = user_stack_size;
    proc->runtime_slot = runtime_slot;
    proc->owns_stack = false;
    proc->task = current;
    return proc;
}

process_t *process_bind_current_user(const char *name,
                                     virt_addr_t user_stack_base,
                                     u64 user_stack_size,
                                     u32 ppid) {
    return process_bind_current_user_image(name,
                                           0,
                                           0,
                                           0,
                                           user_stack_base,
                                           user_stack_size,
                                           0,
                                           ppid);
}

process_t *process_spawn_user_image(const char *name,
                                    virt_addr_t entry,
                                    virt_addr_t text_base,
                                    u64 text_size,
                                    virt_addr_t user_stack_base,
                                    u64 user_stack_size,
                                    u32 runtime_slot,
                                    u32 ppid) {
    if (!name || !entry || !text_base || !text_size || !user_stack_base ||
        !user_stack_size || !runtime_slot) return NULL;

    process_t *proc = proc_find_slot();
    if (!proc) return NULL;
    process_initialise_record(proc, name, ppid);
    proc->entry = entry;
    proc->text_base = text_base;
    proc->text_size = text_size;
    proc->stack_base = user_stack_base;
    proc->stack_size = user_stack_size;
    proc->runtime_slot = runtime_slot;
    proc->owns_stack = false;

    proc->task = task_create(name, process_user_task_entry, 25);
    if (!proc->task) {
        process_zero(proc);
        return NULL;
    }
    return proc;
}

int process_kill(u32 pid, int signal) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid != pid || proc_table[i].state == PROC_STATE_EMPTY) continue;
        if (signal == 9 || signal == 15) {
            proc_table[i].state = PROC_STATE_ZOMBIE;
            proc_table[i].exit_code = -signal;
            fd_close_all(proc_table[i].pid);
            net_socket_close_all(proc_table[i].pid);
            if (proc_table[i].task) task_destroy(proc_table[i].task);
        } else if (signal == 19) {
            proc_table[i].state = PROC_STATE_STOPPED;
        } else if (signal == 18 && proc_table[i].state == PROC_STATE_STOPPED) {
            proc_table[i].state = PROC_STATE_RUNNING;
        }
        return AEGIS_OK;
    }
    return AEGIS_ENOENT;
}

void process_exit(int code) {
    task_t *current = scheduler_current();
    if (!current) return;

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].task != current) continue;
        proc_table[i].state = PROC_STATE_ZOMBIE;
        proc_table[i].exit_code = code;
        fd_close_all(proc_table[i].pid);
        net_socket_close_all(proc_table[i].pid);

        /* Publish the real native exit before the task leaves the CPU.  PID 1
         * is not a supervised child; ENOENT is therefore expected for it.
         */
        (void)service_supervisor_native_mark_exit(proc_table[i].name,
                                                  proc_table[i].pid,
                                                  code);
        task_destroy(current);

        u32 my_pid = proc_table[i].pid;
        for (u32 j = 0; j < MAX_PROCESSES; j++) {
            if (proc_table[j].ppid == my_pid && proc_table[j].state != PROC_STATE_EMPTY) {
                proc_table[j].ppid = 1;
            }
        }
        break;
    }
    scheduler_yield();
    PANIC("dead process resumed after scheduler handoff");
}

int process_wait(u32 pid, int *exit_code) {
    u32 waiter_pid = process_getpid();

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *child = &proc_table[i];
        if (child->pid != pid || child->state == PROC_STATE_EMPTY) continue;
        if (waiter_pid != 0 && child->ppid != waiter_pid) return AEGIS_EPERM;
        if (child->state != PROC_STATE_ZOMBIE) return AEGIS_EBUSY;

        int code = child->exit_code;
        u32 runtime_slot = child->runtime_slot;
        task_t *dead_task = child->task;
        char child_name[TASK_NAME_MAX];
        process_copy_name(child_name, child->name);

        /* The exiting task only marks itself dead.  Once its parent is
         * running, it is safe to unlink and release the kernel task stack.
         */
        if (dead_task) task_destroy(dead_task);
        if (child->owns_stack && child->stack_base) {
            kfree((void *)child->stack_base);
        }
        if (runtime_slot != 0) {
            int release_rc = elf_loader_release_runtime_slot(runtime_slot);
            if (release_rc != AEGIS_OK) return release_rc;
        }

        int userland_rc = userland_mark_feature_native_exited(child_name, pid, code);
        if (userland_rc != AEGIS_OK && userland_rc != AEGIS_ENOENT) return userland_rc;

        if (exit_code) *exit_code = code;
        process_zero(child);
        return AEGIS_OK;
    }
    return AEGIS_ENOENT;
}


int process_wait_any(u32 parent_pid, u32 *child_pid, int *exit_code) {
    if (parent_pid == 0U) parent_pid = process_getpid();
    bool found_child = false;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *child = &proc_table[i];
        if (child->state == PROC_STATE_EMPTY || child->ppid != parent_pid) continue;
        found_child = true;
        if (child->state != PROC_STATE_ZOMBIE) continue;
        u32 pid = child->pid;
        int code = 0;
        int rc = process_wait(pid, &code);
        if (rc != AEGIS_OK) return rc;
        if (child_pid) *child_pid = pid;
        if (exit_code) *exit_code = code;
        return AEGIS_OK;
    }
    return found_child ? AEGIS_EAGAIN : AEGIS_ECHILD;
}

u32 process_child_count(u32 parent_pid) {
    u32 count = 0;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_STATE_EMPTY && proc_table[i].ppid == parent_pid) count++;
    }
    return count;
}

int process_terminate_all_except(u32 keep_pid, int signal) {
    int first_error = AEGIS_OK;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_STATE_EMPTY || p->pid == keep_pid) continue;
        int rc = process_kill(p->pid, signal);
        if (rc != AEGIS_OK && first_error == AEGIS_OK) first_error = rc;
    }
    return first_error;
}

process_t *process_get(u32 pid) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_STATE_EMPTY) return &proc_table[i];
    }
    return NULL;
}

process_t *process_current(void) {
    task_t *current = scheduler_current();
    if (!current) return NULL;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_STATE_EMPTY && proc_table[i].task == current) return &proc_table[i];
    }
    return NULL;
}

u32 process_getpid(void) {
    process_t *p = process_current();
    return p ? p->pid : 0;
}

u32 process_getppid(void) {
    process_t *p = process_current();
    return p ? p->ppid : 0;
}

u32 process_pid(const process_t *proc) {
    return proc ? proc->pid : 0;
}

u32 process_parent_pid(const process_t *proc) {
    return proc ? proc->ppid : 0;
}

const char *process_name(const process_t *proc) {
    return proc ? proc->name : NULL;
}

const char *process_current_name(void) {
    return process_name(process_current());
}

bool process_current_user_read_range_ok(u64 ptr, u64 len) {
    process_t *p = process_current();
    if (!p) return false;
    return elf_loader_slot_read_range_ok(p->runtime_slot, ptr, len) ||
           range_inside(ptr, len, p->stack_base, p->stack_size);
}

bool process_current_user_write_range_ok(u64 ptr, u64 len) {
    process_t *p = process_current();
    if (!p) return false;
    return elf_loader_slot_write_range_ok(p->runtime_slot, ptr, len) ||
           range_inside(ptr, len, p->stack_base, p->stack_size);
}

int process_grant_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return AEGIS_ENOENT;
    for (u32 i = 0; i < p->cap_count; i++) if (p->cap_set[i] == cap) return AEGIS_OK;
    if (p->cap_count >= 16) return AEGIS_ENOMEM;
    p->cap_set[p->cap_count++] = cap;
    return AEGIS_OK;
}

int process_revoke_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return AEGIS_ENOENT;
    for (u32 i = 0; i < p->cap_count; i++) {
        if (p->cap_set[i] == cap) {
            p->cap_set[i] = p->cap_set[--p->cap_count];
            return AEGIS_OK;
        }
    }
    return AEGIS_ENOENT;
}

bool process_has_cap(u32 pid, cap_token_t cap) {
    process_t *p = process_get(pid);
    if (!p) return false;
    for (u32 i = 0; i < p->cap_count; i++) if (p->cap_set[i] == cap) return true;
    return false;
}

process_table_stats_t process_table_stats(void) {
    process_table_stats_t st = {0};
    st.total_slots = MAX_PROCESSES;
    st.next_pid = next_pid;
    st.compact_ready = proc_table_cleanup_ready;

    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY) continue;
        st.used_slots++;
        if (proc_table[i].pid == 1) st.pid1_present = true;
        switch (proc_table[i].state) {
        case PROC_STATE_RUNNING:  st.running++; break;
        case PROC_STATE_SLEEPING: st.sleeping++; break;
        case PROC_STATE_ZOMBIE:   st.zombie++; break;
        case PROC_STATE_STOPPED:  st.stopped++; break;
        default: break;
        }
    }
    return st;
}

static bool process_table_pid_order_is_clean(void) {
    u32 seen[MAX_PROCESSES];
    u32 count = 0;
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY) continue;
        if (proc_table[i].pid == 0) return false;
        for (u32 j = 0; j < count; j++) if (seen[j] == proc_table[i].pid) return false;
        seen[count++] = proc_table[i].pid;
    }
    return true;
}

int process_table_cleanup_prepare(void) {
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY) process_zero(&proc_table[i]);
    }
    if (!process_table_pid_order_is_clean()) return AEGIS_EINVAL;
    proc_table_cleanup_ready = true;
    return AEGIS_OK;
}

int process_table_cleanup_selftest(void) {
    if (!proc_table_cleanup_ready || next_pid == 0 || !process_table_pid_order_is_clean()) return AEGIS_EINVAL;
    process_table_stats_t st = process_table_stats();
    if (st.used_slots > st.total_slots) return AEGIS_EINVAL;
    if ((st.running + st.sleeping + st.zombie + st.stopped) != st.used_slots) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool process_table_cleanup_ready(void) {
    return proc_table_cleanup_ready;
}
