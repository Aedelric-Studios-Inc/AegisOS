/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

typedef struct process process_t;

typedef struct process_table_stats {
    u32 total_slots;
    u32 used_slots;
    u32 running;
    u32 sleeping;
    u32 zombie;
    u32 stopped;
    u32 next_pid;
    bool pid1_present;
    bool compact_ready;
} process_table_stats_t;

void       process_init(void);
process_t *process_create(const char *name, void (*entry)(void), u32 ppid);
process_t *process_bind_current_user(const char *name,
                                     virt_addr_t user_stack_base,
                                     u64 user_stack_size,
                                     u32 ppid);
process_t *process_bind_current_user_image(const char *name,
                                           virt_addr_t entry,
                                           virt_addr_t text_base,
                                           u64 text_size,
                                           virt_addr_t user_stack_base,
                                           u64 user_stack_size,
                                           u32 runtime_slot,
                                           u32 ppid);
process_t *process_spawn_user_image(const char *name,
                                    virt_addr_t entry,
                                    virt_addr_t text_base,
                                    u64 text_size,
                                    virt_addr_t user_stack_base,
                                    u64 user_stack_size,
                                    u32 runtime_slot,
                                    u32 ppid);
int        process_kill(u32 pid, int signal);
void       process_exit(int code);
int        process_wait(u32 pid, int *exit_code);
int        process_wait_any(u32 parent_pid, u32 *child_pid, int *exit_code);
int        process_terminate_all_except(u32 keep_pid, int signal);
u32        process_child_count(u32 parent_pid);
process_t *process_get(u32 pid);
process_t *process_current(void);
u32        process_getpid(void);
u32        process_getppid(void);
u32        process_pid(const process_t *proc);
u32        process_parent_pid(const process_t *proc);
const char *process_name(const process_t *proc);
const char *process_current_name(void);
bool       process_current_user_read_range_ok(u64 ptr, u64 len);
bool       process_current_user_write_range_ok(u64 ptr, u64 len);
int        process_grant_cap(u32 pid, cap_token_t cap);
int        process_revoke_cap(u32 pid, cap_token_t cap);
bool       process_has_cap(u32 pid, cap_token_t cap);
int        process_table_cleanup_prepare(void);
int        process_table_cleanup_selftest(void);
process_table_stats_t process_table_stats(void);
bool       process_table_cleanup_ready(void);
