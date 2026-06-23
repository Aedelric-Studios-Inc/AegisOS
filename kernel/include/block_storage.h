/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS v41 block/storage abstraction.
 *
 * This is the first kernel-visible storage contract above the v40 flash-image
 * layout.  It does not yet perform hardware I/O; it records the boot/root/config
 * regions as block devices/partitions so the scheduler, VFS, configfs, and
 * future storage drivers have a stable target model.
 */

#include "types.h"

#define AEGIS_BLOCK_DEVICE_MAX       8
#define AEGIS_BLOCK_PARTITION_MAX    16
#define AEGIS_BLOCK_NAME_MAX         32

#define AEGIS_BLOCK_FLAG_READABLE    (1u << 0)
#define AEGIS_BLOCK_FLAG_WRITABLE    (1u << 1)
#define AEGIS_BLOCK_FLAG_BOOT        (1u << 2)
#define AEGIS_BLOCK_FLAG_ROOT        (1u << 3)
#define AEGIS_BLOCK_FLAG_CONFIG      (1u << 4)
#define AEGIS_BLOCK_FLAG_PERSISTENT  (1u << 5)

typedef enum aegis_block_device_state {
    AEGIS_BLOCK_DEVICE_EMPTY = 0,
    AEGIS_BLOCK_DEVICE_REGISTERED,
    AEGIS_BLOCK_DEVICE_READY,
} aegis_block_device_state_t;

typedef enum aegis_block_partition_state {
    AEGIS_BLOCK_PARTITION_EMPTY = 0,
    AEGIS_BLOCK_PARTITION_DECLARED,
    AEGIS_BLOCK_PARTITION_READY,
} aegis_block_partition_state_t;

typedef struct aegis_block_device {
    u32 id;
    char name[AEGIS_BLOCK_NAME_MAX];
    aegis_block_device_state_t state;
    u64 block_size;
    u64 block_count;
    u32 flags;
} aegis_block_device_t;

typedef struct aegis_block_partition {
    u32 id;
    u32 device_id;
    char name[AEGIS_BLOCK_NAME_MAX];
    aegis_block_partition_state_t state;
    u64 start_lba;
    u64 block_count;
    u32 flags;
} aegis_block_partition_t;

typedef struct aegis_block_registry {
    bool initialised;
    bool layout_ready;
    bool persistent_config_ready;
    u32 device_count;
    u32 partition_count;
    u32 ready_partition_count;
    u32 persistent_partition_count;
} aegis_block_registry_t;

void block_storage_init(void);
int  block_storage_register_v40_flash_layout(void);
int  block_storage_selftest(void);
bool block_storage_layout_ready(void);
bool block_storage_persistent_config_ready(void);
const aegis_block_registry_t *block_storage_state(void);
const aegis_block_device_t *block_storage_device(u32 index);
const aegis_block_partition_t *block_storage_partition(u32 index);
const aegis_block_partition_t *block_storage_find_partition(const char *name);
const char *block_storage_device_state_name(aegis_block_device_state_t state);
const char *block_storage_partition_state_name(aegis_block_partition_state_t state);
