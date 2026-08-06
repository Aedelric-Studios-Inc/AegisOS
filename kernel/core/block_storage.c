/* SPDX-License-Identifier: Proprietary */
#include "block_storage.h"

static aegis_block_device_t devices[AEGIS_BLOCK_DEVICE_MAX];
static aegis_block_partition_t partitions[AEGIS_BLOCK_PARTITION_MAX];
static aegis_block_registry_t registry;
static u32 next_device_id;
static u32 next_partition_id;

static void zero_mem(void *ptr, u64 len) { u8 *p = (u8 *)ptr; for (u64 i = 0; i < len; i++) p[i] = 0; }
static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) { if (*a++ != *b++) return false; }
    return *a == '\0' && *b == '\0';
}
static void copy_name(char *dst, u32 cap, const char *src) {
    u32 i = 0; if (!dst || cap == 0U) return; if (!src) src = "unnamed";
    while (src[i] && i + 1U < cap) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

void block_storage_init(void) {
    zero_mem(devices, sizeof(devices));
    zero_mem(partitions, sizeof(partitions));
    zero_mem(&registry, sizeof(registry));
    next_device_id = 1U;
    next_partition_id = 1U;
    registry.initialised = true;
}

int block_storage_register_device(const char *name, u64 block_size, u64 block_count,
                                  u32 flags, const aegis_block_ops_t *ops,
                                  void *driver_ctx, u32 *out_id) {
    if (!registry.initialised || !name || block_size == 0U || block_count == 0U) return AEGIS_EINVAL;
    if (registry.device_count >= AEGIS_BLOCK_DEVICE_MAX) return AEGIS_ENOMEM;
    aegis_block_device_t *d = &devices[registry.device_count++];
    zero_mem(d, sizeof(*d));
    d->id = next_device_id++;
    copy_name(d->name, sizeof(d->name), name);
    d->block_size = block_size;
    d->block_count = block_count;
    d->flags = flags | AEGIS_BLOCK_FLAG_READABLE;
    d->ops = ops;
    d->driver_ctx = driver_ctx;
    d->state = AEGIS_BLOCK_DEVICE_READY;
    if (ops && ops->read) registry.io_backend_ready = true;
    if (out_id) *out_id = d->id;
    return AEGIS_OK;
}

int block_storage_register_partition(u32 device_id, const char *name,
                                     u64 start_lba, u64 block_count, u32 flags) {
    if (!registry.initialised || !name || device_id == 0U || block_count == 0U) return AEGIS_EINVAL;
    if (registry.partition_count >= AEGIS_BLOCK_PARTITION_MAX) return AEGIS_ENOMEM;
    aegis_block_device_t *dev = block_storage_device_by_id(device_id);
    if (!dev || dev->state != AEGIS_BLOCK_DEVICE_READY) return AEGIS_ENOENT;
    if (start_lba >= dev->block_count || block_count > dev->block_count - start_lba) return AEGIS_EINVAL;
    aegis_block_partition_t *p = &partitions[registry.partition_count++];
    zero_mem(p, sizeof(*p));
    p->id = next_partition_id++;
    p->device_id = device_id;
    copy_name(p->name, sizeof(p->name), name);
    p->state = AEGIS_BLOCK_PARTITION_READY;
    p->start_lba = start_lba;
    p->block_count = block_count;
    p->flags = flags | AEGIS_BLOCK_FLAG_READABLE;
    registry.ready_partition_count++;
    if (p->flags & AEGIS_BLOCK_FLAG_PERSISTENT) registry.persistent_partition_count++;
    return AEGIS_OK;
}

int block_storage_register_v40_flash_layout(void) {
    if (!registry.initialised) return AEGIS_EINVAL;
    if (registry.layout_ready) return AEGIS_OK;
    u32 dev_id = 0U;
    if (registry.device_count != 0U) {
        dev_id = devices[0].id;
    } else {
        int rc = block_storage_register_device("aegis-flash0", 512U, 524288U,
                                               AEGIS_BLOCK_FLAG_WRITABLE, NULL, NULL, &dev_id);
        if (rc != AEGIS_OK) return rc;
    }
    aegis_block_device_t *dev = block_storage_device_by_id(dev_id);
    if (!dev || dev->block_count < 460800U) return AEGIS_EINVAL;
    int rc = block_storage_register_partition(dev_id, "AEGIS_BOOT", 2048U, 32768U, AEGIS_BLOCK_FLAG_BOOT);
    if (rc != AEGIS_OK) return rc;
    rc = block_storage_register_partition(dev_id, "AEGIS_ROOT", 67584U, 327680U,
                                          AEGIS_BLOCK_FLAG_ROOT | AEGIS_BLOCK_FLAG_WRITABLE);
    if (rc != AEGIS_OK) return rc;
    rc = block_storage_register_partition(dev_id, "AEGIS_CONFIG", 395264U, 65536U,
                                          AEGIS_BLOCK_FLAG_CONFIG | AEGIS_BLOCK_FLAG_WRITABLE | AEGIS_BLOCK_FLAG_PERSISTENT);
    if (rc != AEGIS_OK) return rc;
    registry.layout_ready = true;
    registry.persistent_config_ready = block_storage_find_partition("AEGIS_CONFIG") != NULL;
    return registry.persistent_config_ready ? AEGIS_OK : AEGIS_ENOENT;
}

aegis_block_device_t *block_storage_device_by_id(u32 id) {
    for (u32 i = 0; i < registry.device_count; i++) if (devices[i].id == id) return &devices[i];
    return NULL;
}

int block_storage_read(u32 device_id, u64 lba, u32 count, void *buffer) {
    aegis_block_device_t *d = block_storage_device_by_id(device_id);
    if (!d || !buffer || count == 0U || lba >= d->block_count || count > d->block_count - lba) return AEGIS_EINVAL;
    if (!d->ops || !d->ops->read) return AEGIS_ENOSYS;
    int rc = d->ops->read(d->driver_ctx, lba, count, buffer);
    if (rc == AEGIS_OK) d->reads += count; else d->io_errors++;
    return rc;
}
int block_storage_write(u32 device_id, u64 lba, u32 count, const void *buffer) {
    aegis_block_device_t *d = block_storage_device_by_id(device_id);
    if (!d || !buffer || count == 0U || !(d->flags & AEGIS_BLOCK_FLAG_WRITABLE) ||
        lba >= d->block_count || count > d->block_count - lba) return AEGIS_EINVAL;
    if (!d->ops || !d->ops->write) return AEGIS_ENOSYS;
    int rc = d->ops->write(d->driver_ctx, lba, count, buffer);
    if (rc == AEGIS_OK) d->writes += count; else d->io_errors++;
    return rc;
}
int block_storage_flush(u32 device_id) {
    aegis_block_device_t *d = block_storage_device_by_id(device_id);
    if (!d) return AEGIS_ENOENT;
    return d->ops && d->ops->flush ? d->ops->flush(d->driver_ctx) : AEGIS_OK;
}
int block_partition_read(const aegis_block_partition_t *p, u64 rel, u32 count, void *buffer) {
    if (!p || rel >= p->block_count || count > p->block_count - rel) return AEGIS_EINVAL;
    return block_storage_read(p->device_id, p->start_lba + rel, count, buffer);
}
int block_partition_write(const aegis_block_partition_t *p, u64 rel, u32 count, const void *buffer) {
    if (!p || !(p->flags & AEGIS_BLOCK_FLAG_WRITABLE) || rel >= p->block_count || count > p->block_count - rel) return AEGIS_EINVAL;
    return block_storage_write(p->device_id, p->start_lba + rel, count, buffer);
}

static bool non_overlap(const aegis_block_partition_t *a, const aegis_block_partition_t *b) {
    u64 ae = a->start_lba + a->block_count, be = b->start_lba + b->block_count;
    return ae > a->start_lba && be > b->start_lba && (ae <= b->start_lba || be <= a->start_lba);
}
int block_storage_selftest(void) {
    if (!registry.initialised || !registry.layout_ready || !registry.persistent_config_ready) return AEGIS_EINVAL;
    if (!block_storage_find_partition("AEGIS_BOOT") || !block_storage_find_partition("AEGIS_ROOT") ||
        !block_storage_find_partition("AEGIS_CONFIG")) return AEGIS_ENOENT;
    for (u32 i = 0; i < registry.partition_count; i++) {
        const aegis_block_partition_t *p = &partitions[i];
        if (p->state != AEGIS_BLOCK_PARTITION_READY || p->block_count == 0U) return AEGIS_EINVAL;
        for (u32 j = i + 1U; j < registry.partition_count; j++) if (!non_overlap(p, &partitions[j])) return AEGIS_EINVAL;
    }
    return AEGIS_OK;
}

bool block_storage_layout_ready(void) { return registry.layout_ready; }
bool block_storage_persistent_config_ready(void) { return registry.persistent_config_ready; }
bool block_storage_io_backend_ready(void) { return registry.io_backend_ready; }
const aegis_block_registry_t *block_storage_state(void) { return &registry; }
const aegis_block_device_t *block_storage_device(u32 index) { return index < registry.device_count ? &devices[index] : NULL; }
const aegis_block_partition_t *block_storage_partition(u32 index) { return index < registry.partition_count ? &partitions[index] : NULL; }
const aegis_block_partition_t *block_storage_find_partition(const char *name) {
    for (u32 i = 0; i < registry.partition_count; i++) if (partitions[i].state != AEGIS_BLOCK_PARTITION_EMPTY && str_eq(partitions[i].name, name)) return &partitions[i];
    return NULL;
}
const char *block_storage_device_state_name(aegis_block_device_state_t s) {
    switch (s) { case AEGIS_BLOCK_DEVICE_EMPTY: return "empty"; case AEGIS_BLOCK_DEVICE_REGISTERED: return "registered"; case AEGIS_BLOCK_DEVICE_READY: return "ready"; case AEGIS_BLOCK_DEVICE_FAULTED: return "faulted"; default: return "unknown"; }
}
const char *block_storage_partition_state_name(aegis_block_partition_state_t s) {
    switch (s) { case AEGIS_BLOCK_PARTITION_EMPTY: return "empty"; case AEGIS_BLOCK_PARTITION_DECLARED: return "declared"; case AEGIS_BLOCK_PARTITION_READY: return "ready"; default: return "unknown"; }
}
