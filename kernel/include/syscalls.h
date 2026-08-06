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
#define SYS_CHANNEL_OPEN 18
#define SYS_SERVICE_READY 19
#define SYS_CONSOLE_READY 20
#define SYS_KILL        21
#define SYS_SLEEP       22
#define SYS_CLOCK_GET   23
#define SYS_SHUTDOWN    24
#define SYS_SOCKET      25
#define SYS_BIND        26
#define SYS_LISTEN      27
#define SYS_ACCEPT      28
#define SYS_CONNECT     29
#define SYS_SEND        30
#define SYS_RECV        31
#define SYS_RANDOM      32
#define NR_SYSCALLS     33

#define AEGIS_WAIT_NOHANG 0x1U

#define AEGIS_CLOCK_MONOTONIC 1U
#define AEGIS_CLOCK_REALTIME  2U

#define AEGIS_SHUTDOWN_POWEROFF 0U
#define AEGIS_SHUTDOWN_REBOOT   1U

typedef s64 (*syscall_fn_t)(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);

void  syscall_table_init(void);
s64   syscall_dispatch(u64 nr, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);
