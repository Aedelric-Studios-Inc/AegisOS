/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"
int  logfs_mount(const char *path);
int  logfs_append(const char *tag, const char *msg);
int  logfs_read_last(u32 n, char *buf, u32 buf_len);
