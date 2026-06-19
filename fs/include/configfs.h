/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"
int  configfs_mount(const char *path);
int  configfs_get(const char *key, char *value, u32 max_len);
int  configfs_set(const char *key, const char *value);
