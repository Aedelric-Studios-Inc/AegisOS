/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

#define AEGIS_FD_FIRST 3
#define AEGIS_FD_MAX_PER_PROCESS 32

void fd_table_init(void);
int  fd_install(u32 pid, void *object, bool readable, bool writable);
void *fd_get(u32 pid, int fd, bool for_write, u64 **offset_out);
int  fd_close(u32 pid, int fd);
void fd_close_all(u32 pid);
