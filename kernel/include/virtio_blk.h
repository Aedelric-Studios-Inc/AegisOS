/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

int  virtio_blk_init(void);
bool virtio_blk_ready(void);
u64  virtio_blk_capacity_sectors(void);
int  virtio_blk_read_sectors(u64 sector, u32 count, void *buffer);
int  virtio_blk_write_sectors(u64 sector, u32 count, const void *buffer);
int  virtio_blk_flush(void);
