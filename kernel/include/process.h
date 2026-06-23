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
int        process_kill(u32 pid, int signal);
void       process_exit(int code);
int        process_wait(u32 pid, int *exit_code);
process_t *process_get(u32 pid);
process_t *process_current(void);
u32        process_getpid(void);
int        process_grant_cap(u32 pid, cap_token_t cap);
int        process_revoke_cap(u32 pid, cap_token_t cap);
bool       process_has_cap(u32 pid, cap_token_t cap);
int        process_table_cleanup_prepare(void);
int        process_table_cleanup_selftest(void);
process_table_stats_t process_table_stats(void);
bool       process_table_cleanup_ready(void);
