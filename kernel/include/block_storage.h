/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

#define AEGIS_BLOCK_DEVICE_MAX       8U
#define AEGIS_BLOCK_PARTITION_MAX    16U
#define AEGIS_BLOCK_NAME_MAX         32U

#define AEGIS_BLOCK_FLAG_READABLE    (1U << 0)
#define AEGIS_BLOCK_FLAG_WRITABLE    (1U << 1)
#define AEGIS_BLOCK_FLAG_BOOT        (1U << 2)
#define AEGIS_BLOCK_FLAG_ROOT        (1U << 3)
#define AEGIS_BLOCK_FLAG_CONFIG      (1U << 4)
#define AEGIS_BLOCK_FLAG_PERSISTENT  (1U << 5)
#define AEGIS_BLOCK_FLAG_VIRTIO      (1U << 6)
#define AEGIS_BLOCK_FLAG_NVME        (1U << 7)

typedef enum aegis_block_device_state {
    AEGIS_BLOCK_DEVICE_EMPTY = 0,
    AEGIS_BLOCK_DEVICE_REGISTERED,
    AEGIS_BLOCK_DEVICE_READY,
    AEGIS_BLOCK_DEVICE_FAULTED,
} aegis_block_device_state_t;

typedef enum aegis_block_partition_state {
    AEGIS_BLOCK_PARTITION_EMPTY = 0,
    AEGIS_BLOCK_PARTITION_DECLARED,
    AEGIS_BLOCK_PARTITION_READY,
} aegis_block_partition_state_t;

typedef int (*aegis_block_read_fn)(void *ctx, u64 lba, u32 count, void *buffer);
typedef int (*aegis_block_write_fn)(void *ctx, u64 lba, u32 count, const void *buffer);
typedef int (*aegis_block_flush_fn)(void *ctx);

typedef struct aegis_block_ops {
    aegis_block_read_fn read;
    aegis_block_write_fn write;
    aegis_block_flush_fn flush;
} aegis_block_ops_t;

typedef struct aegis_block_device {
    u32 id;
    char name[AEGIS_BLOCK_NAME_MAX];
    aegis_block_device_state_t state;
    u64 block_size;
    u64 block_count;
    u32 flags;
    const aegis_block_ops_t *ops;
    void *driver_ctx;
    u64 reads;
    u64 writes;
    u64 io_errors;
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
    bool io_backend_ready;
    u32 device_count;
    u32 partition_count;
    u32 ready_partition_count;
    u32 persistent_partition_count;
} aegis_block_registry_t;

void block_storage_init(void);
int  block_storage_register_device(const char *name, u64 block_size, u64 block_count,
                                   u32 flags, const aegis_block_ops_t *ops,
                                   void *driver_ctx, u32 *out_id);
int  block_storage_register_partition(u32 device_id, const char *name,
                                      u64 start_lba, u64 block_count, u32 flags);
int  block_storage_register_v40_flash_layout(void);
int  block_storage_selftest(void);
int  block_storage_read(u32 device_id, u64 lba, u32 count, void *buffer);
int  block_storage_write(u32 device_id, u64 lba, u32 count, const void *buffer);
int  block_storage_flush(u32 device_id);
int  block_partition_read(const aegis_block_partition_t *part, u64 relative_lba,
                          u32 count, void *buffer);
int  block_partition_write(const aegis_block_partition_t *part, u64 relative_lba,
                           u32 count, const void *buffer);

bool block_storage_layout_ready(void);
bool block_storage_persistent_config_ready(void);
bool block_storage_io_backend_ready(void);
const aegis_block_registry_t *block_storage_state(void);
const aegis_block_device_t *block_storage_device(u32 index);
aegis_block_device_t *block_storage_device_by_id(u32 id);
const aegis_block_partition_t *block_storage_partition(u32 index);
const aegis_block_partition_t *block_storage_find_partition(const char *name);
const char *block_storage_device_state_name(aegis_block_device_state_t state);
const char *block_storage_partition_state_name(aegis_block_partition_state_t state);
