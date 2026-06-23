/* SPDX-License-Identifier: Proprietary */
#pragma once
/* syscalls.h — system call numbers and dispatch interface */

#include "types.h"

#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_EXIT        4
#define SYS_GETPID      5
#define SYS_MMAP        6
#define SYS_MUNMAP      7
#define SYS_SEND_MSG    8
#define SYS_RECV_MSG    9
#define SYS_CAP_GRANT   10
#define SYS_CAP_REVOKE  11
#define SYS_YIELD       12
#define SYS_SPAWN       13
#define SYS_WAITPID     14
#define SYS_GETPPID     15
#define SYS_GETTID      16
#define SYS_SERVICE_ID  17
#define NR_SYSCALLS     18

typedef s64 (*syscall_fn_t)(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);

void  syscall_table_init(void);
s64   syscall_dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);
