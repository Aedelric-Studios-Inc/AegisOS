/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/syscall_table.c
 * System call dispatch table.
 */

#include "syscalls.h"
#include "types.h"

static s64 sys_unimplemented(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return AEGIS_EINVAL;
}

/* Forward declarations for individual syscall implementations */
s64 sys_read(u64, u64, u64, u64, u64, u64);
s64 sys_write(u64, u64, u64, u64, u64, u64);
s64 sys_open(u64, u64, u64, u64, u64, u64);
s64 sys_close(u64, u64, u64, u64, u64, u64);
s64 sys_exit(u64, u64, u64, u64, u64, u64);
s64 sys_getpid(u64, u64, u64, u64, u64, u64);
s64 sys_mmap(u64, u64, u64, u64, u64, u64);
s64 sys_munmap(u64, u64, u64, u64, u64, u64);
s64 sys_send_msg(u64, u64, u64, u64, u64, u64);
s64 sys_recv_msg(u64, u64, u64, u64, u64, u64);
s64 sys_cap_grant(u64, u64, u64, u64, u64, u64);
s64 sys_cap_revoke(u64, u64, u64, u64, u64, u64);
s64 sys_yield(u64, u64, u64, u64, u64, u64);
s64 sys_spawn(u64, u64, u64, u64, u64, u64);
s64 sys_waitpid(u64, u64, u64, u64, u64, u64);
s64 sys_getppid(u64, u64, u64, u64, u64, u64);
s64 sys_gettid(u64, u64, u64, u64, u64, u64);
s64 sys_service_id(u64, u64, u64, u64, u64, u64);

static syscall_fn_t syscall_table[NR_SYSCALLS];

void syscall_table_init(void) {
    for (int i = 0; i < NR_SYSCALLS; i++)
        syscall_table[i] = sys_unimplemented;

    syscall_table[SYS_READ]       = sys_read;
    syscall_table[SYS_WRITE]      = sys_write;
    syscall_table[SYS_OPEN]       = sys_open;
    syscall_table[SYS_CLOSE]      = sys_close;
    syscall_table[SYS_EXIT]       = sys_exit;
    syscall_table[SYS_GETPID]     = sys_getpid;
    syscall_table[SYS_MMAP]       = sys_mmap;
    syscall_table[SYS_MUNMAP]     = sys_munmap;
    syscall_table[SYS_SEND_MSG]   = sys_send_msg;
    syscall_table[SYS_RECV_MSG]   = sys_recv_msg;
    syscall_table[SYS_CAP_GRANT]  = sys_cap_grant;
    syscall_table[SYS_CAP_REVOKE] = sys_cap_revoke;
    syscall_table[SYS_YIELD]      = sys_yield;
    syscall_table[SYS_SPAWN]      = sys_spawn;
    syscall_table[SYS_WAITPID]    = sys_waitpid;
    syscall_table[SYS_GETPPID]    = sys_getppid;
    syscall_table[SYS_GETTID]     = sys_gettid;
    syscall_table[SYS_SERVICE_ID] = sys_service_id;
}

s64 syscall_dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    if (nr >= NR_SYSCALLS) return AEGIS_EINVAL;
    return syscall_table[nr](a0, a1, a2, a3, a4, a5);
}
