/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/core/block_storage.c
 * v41 basic block/storage abstraction for the v40 flash image layout.
 */

#include "block_storage.h"

static aegis_block_device_t devices[AEGIS_BLOCK_DEVICE_MAX];
static aegis_block_partition_t partitions[AEGIS_BLOCK_PARTITION_MAX];
static aegis_block_registry_t registry;
static u32 next_device_id;
static u32 next_partition_id;

static void zero_mem(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void copy_name(char *dst, u32 dst_len, const char *src) {
    u32 i = 0;
    if (!dst || dst_len == 0) return;
    if (!src) src = "unnamed";
    while (src[i] && i < dst_len - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void block_storage_init(void) {
    zero_mem(devices, sizeof(devices));
    zero_mem(partitions, sizeof(partitions));
    zero_mem(&registry, sizeof(registry));
    next_device_id = 1;
    next_partition_id = 1;
    registry.initialised = true;
}

static int register_device(const char *name, u64 block_size, u64 block_count, u32 flags, u32 *out_id) {
    if (!registry.initialised || !name || block_size == 0 || block_count == 0) return AEGIS_EINVAL;
    if (registry.device_count >= AEGIS_BLOCK_DEVICE_MAX) return AEGIS_ENOMEM;

    aegis_block_device_t *d = &devices[registry.device_count];
    d->id = next_device_id++;
    copy_name(d->name, AEGIS_BLOCK_NAME_MAX, name);
    d->state = AEGIS_BLOCK_DEVICE_READY;
    d->block_size = block_size;
    d->block_count = block_count;
    d->flags = flags | AEGIS_BLOCK_FLAG_READABLE;
    registry.device_count++;
    if (out_id) *out_id = d->id;
    return AEGIS_OK;
}

static int register_partition(u32 device_id, const char *name, u64 start_lba, u64 block_count, u32 flags) {
    if (!registry.initialised || !name || device_id == 0 || block_count == 0) return AEGIS_EINVAL;
    if (registry.partition_count >= AEGIS_BLOCK_PARTITION_MAX) return AEGIS_ENOMEM;

    const aegis_block_device_t *dev = NULL;
    for (u32 i = 0; i < registry.device_count; i++) {
        if (devices[i].id == device_id && devices[i].state == AEGIS_BLOCK_DEVICE_READY) {
            dev = &devices[i];
            break;
        }
    }
    if (!dev) return AEGIS_ENOENT;
    if (start_lba >= dev->block_count || block_count > dev->block_count - start_lba) return AEGIS_EINVAL;

    aegis_block_partition_t *p = &partitions[registry.partition_count];
    p->id = next_partition_id++;
    p->device_id = device_id;
    copy_name(p->name, AEGIS_BLOCK_NAME_MAX, name);
    p->state = AEGIS_BLOCK_PARTITION_READY;
    p->start_lba = start_lba;
    p->block_count = block_count;
    p->flags = flags | AEGIS_BLOCK_FLAG_READABLE;
    registry.partition_count++;
    registry.ready_partition_count++;
    if ((p->flags & AEGIS_BLOCK_FLAG_PERSISTENT) != 0) registry.persistent_partition_count++;
    return AEGIS_OK;
}

int block_storage_register_v40_flash_layout(void) {
    if (!registry.initialised) return AEGIS_EINVAL;
    if (registry.layout_ready) return AEGIS_OK;

    u32 dev_id = 0;
    int rc = register_device("aegis-flash0", 512, 524288,
                             AEGIS_BLOCK_FLAG_READABLE | AEGIS_BLOCK_FLAG_WRITABLE,
                             &dev_id);
    if (rc != AEGIS_OK) return rc;

    rc = register_partition(dev_id, "AEGIS_BOOT", 2048, 32768,
                            AEGIS_BLOCK_FLAG_BOOT);
    if (rc != AEGIS_OK) return rc;
    rc = register_partition(dev_id, "AEGIS_ROOT", 67584, 327680,
                            AEGIS_BLOCK_FLAG_ROOT | AEGIS_BLOCK_FLAG_WRITABLE);
    if (rc != AEGIS_OK) return rc;
    rc = register_partition(dev_id, "AEGIS_CONFIG", 395264, 65536,
                            AEGIS_BLOCK_FLAG_CONFIG | AEGIS_BLOCK_FLAG_WRITABLE | AEGIS_BLOCK_FLAG_PERSISTENT);
    if (rc != AEGIS_OK) return rc;

    registry.layout_ready = true;
    registry.persistent_config_ready = (block_storage_find_partition("AEGIS_CONFIG") != 0);
    return registry.persistent_config_ready ? AEGIS_OK : AEGIS_ENOENT;
}

static bool ranges_do_not_overlap(const aegis_block_partition_t *a, const aegis_block_partition_t *b) {
    u64 a_end = a->start_lba + a->block_count;
    u64 b_end = b->start_lba + b->block_count;
    if (a_end <= a->start_lba || b_end <= b->start_lba) return false;
    return a_end <= b->start_lba || b_end <= a->start_lba;
}

int block_storage_selftest(void) {
    if (!registry.initialised || !registry.layout_ready || !registry.persistent_config_ready) return AEGIS_EINVAL;
    if (registry.device_count != 1 || registry.partition_count != 3) return AEGIS_EINVAL;
    if (!block_storage_find_partition("AEGIS_BOOT") ||
        !block_storage_find_partition("AEGIS_ROOT") ||
        !block_storage_find_partition("AEGIS_CONFIG")) return AEGIS_ENOENT;

    for (u32 i = 0; i < registry.partition_count; i++) {
        const aegis_block_partition_t *p = &partitions[i];
        if (p->state != AEGIS_BLOCK_PARTITION_READY || p->block_count == 0) return AEGIS_EINVAL;
        for (u32 j = i + 1; j < registry.partition_count; j++) {
            if (!ranges_do_not_overlap(p, &partitions[j])) return AEGIS_EINVAL;
        }
    }
    const aegis_block_partition_t *cfg = block_storage_find_partition("AEGIS_CONFIG");
    if (!cfg || (cfg->flags & AEGIS_BLOCK_FLAG_PERSISTENT) == 0 ||
        (cfg->flags & AEGIS_BLOCK_FLAG_WRITABLE) == 0) return AEGIS_EINVAL;
    return AEGIS_OK;
}

bool block_storage_layout_ready(void) { return registry.layout_ready; }
bool block_storage_persistent_config_ready(void) { return registry.persistent_config_ready; }
const aegis_block_registry_t *block_storage_state(void) { return &registry; }

const aegis_block_device_t *block_storage_device(u32 index) {
    if (index >= registry.device_count) return 0;
    return &devices[index];
}

const aegis_block_partition_t *block_storage_partition(u32 index) {
    if (index >= registry.partition_count) return 0;
    return &partitions[index];
}

const aegis_block_partition_t *block_storage_find_partition(const char *name) {
    for (u32 i = 0; i < registry.partition_count; i++) {
        if (partitions[i].state != AEGIS_BLOCK_PARTITION_EMPTY && str_eq(partitions[i].name, name)) return &partitions[i];
    }
    return 0;
}

const char *block_storage_device_state_name(aegis_block_device_state_t state) {
    switch (state) {
    case AEGIS_BLOCK_DEVICE_EMPTY: return "empty";
    case AEGIS_BLOCK_DEVICE_REGISTERED: return "registered";
    case AEGIS_BLOCK_DEVICE_READY: return "ready";
    default: return "unknown";
    }
}

const char *block_storage_partition_state_name(aegis_block_partition_state_t state) {
    switch (state) {
    case AEGIS_BLOCK_PARTITION_EMPTY: return "empty";
    case AEGIS_BLOCK_PARTITION_DECLARED: return "declared";
    case AEGIS_BLOCK_PARTITION_READY: return "ready";
    default: return "unknown";
    }
}
