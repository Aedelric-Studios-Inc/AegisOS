/* SPDX-License-Identifier: Proprietary */
/* AegisOS — fs/logfs.c */
#include "include/logfs.h"

int logfs_mount(const char *path) { (void)path; return 0; }
int logfs_append(const char *tag, const char *msg) { (void)tag; (void)msg; return 0; }
int logfs_read_last(u32 n, char *buf, u32 buf_len) { (void)n; (void)buf; (void)buf_len; return 0; }
