/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"

int  aegisfs_mount_persistent(void);
bool aegisfs_is_mounted(void);
bool aegisfs_is_writable(void);
u32  aegisfs_file_count(void);
u32  aegisfs_generation(void);
int  aegisfs_sync(void);
int  aegisfs_selftest(void);
