/* SPDX-License-Identifier: Proprietary */
#pragma once
/* storage.h — Block storage driver interface */

#include "../../kernel/include/types.h"

#define BLOCK_SIZE 512

typedef struct {
    u32 block_count;
    u32 block_size;
    char label[16];
} storage_info_t;

int storage_init(void);
int storage_read(u32 lba, u32 count, void *buf);
int storage_write(u32 lba, u32 count, const void *buf);
int storage_get_info(storage_info_t *info);
