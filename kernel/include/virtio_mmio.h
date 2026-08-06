/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "types.h"

#define VIRTIO_MMIO_MAGIC_VALUE 0x74726976U
#define VIRTIO_DEVICE_NET       1U
#define VIRTIO_DEVICE_BLOCK     2U
#define VIRTIO_DEVICE_RNG       4U
#define VIRTIO_F_VERSION_1       (1ULL << 32)
#define VIRTIO_MMIO_MAX_DEVICES 16U
#define VIRTQ_DESC_F_NEXT       1U
#define VIRTQ_DESC_F_WRITE      2U

typedef struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed)) virtq_desc_t;

typedef struct virtq_avail {
    u16 flags;
    u16 idx;
    u16 ring[256];
    u16 used_event;
} __attribute__((packed)) virtq_avail_t;

typedef struct virtq_used_elem {
    u32 id;
    u32 len;
} __attribute__((packed)) virtq_used_elem_t;

typedef struct virtq_used {
    u16 flags;
    u16 idx;
    virtq_used_elem_t ring[256];
    u16 avail_event;
} __attribute__((packed, aligned(4))) virtq_used_t;

typedef struct virtio_mmio_device {
    u64 base;
    u32 irq;
    u32 version;
    u32 device_id;
    u32 vendor_id;
    bool present;
    bool driver_ok;
} virtio_mmio_device_t;

typedef struct virtio_queue {
    virtio_mmio_device_t *device;
    u16 index;
    u16 size;
    u16 last_used_idx;
    virtq_desc_t *desc;
    virtq_avail_t *avail;
    virtq_used_t *used;
} virtio_queue_t;

/*
 * Split-ring memory is DMA-owned by the device and little-endian.  Never cast
 * a packed ring address to u16/u32: queue containers may otherwise place the
 * ring at only two-byte alignment, and AArch64 alignment checking will fault.
 * Byte loads are valid for every DMA address and keep the access volatile.
 */
static inline u16 virtq_dma_load_le16(const volatile void *address) {
    const volatile u8 *bytes = (const volatile u8 *)address;
    return (u16)((u16)bytes[0] | ((u16)bytes[1] << 8));
}

static inline u32 virtq_dma_load_le32(const volatile void *address) {
    const volatile u8 *bytes = (const volatile u8 *)address;
    return (u32)bytes[0]
         | ((u32)bytes[1] << 8)
         | ((u32)bytes[2] << 16)
         | ((u32)bytes[3] << 24);
}

static inline u16 virtq_used_load_idx(const virtq_used_t *used) {
    const volatile u8 *bytes = (const volatile u8 *)(const volatile void *)used;
    return virtq_dma_load_le16(bytes + 2U);
}

static inline u32 virtq_used_elem_load_id(const virtq_used_elem_t *elem) {
    return virtq_dma_load_le32((const volatile void *)elem);
}

static inline u32 virtq_used_elem_load_len(const virtq_used_elem_t *elem) {
    const volatile u8 *bytes = (const volatile u8 *)(const volatile void *)elem;
    return virtq_dma_load_le32(bytes + 4U);
}

void virtio_mmio_bus_init(void);
int  virtio_mmio_discover(void);
u32  virtio_mmio_device_count(void);
virtio_mmio_device_t *virtio_mmio_device_by_type(u32 device_id, u32 ordinal);
int  virtio_mmio_begin(virtio_mmio_device_t *dev, u64 accepted_features);
int  virtio_mmio_setup_queue(virtio_mmio_device_t *dev, virtio_queue_t *q,
                             u16 index, u16 requested_size,
                             virtq_desc_t *desc, virtq_avail_t *avail,
                             virtq_used_t *used);
void virtio_mmio_finish(virtio_mmio_device_t *dev);
void virtio_mmio_notify(const virtio_queue_t *q);
u32  virtio_mmio_interrupt_status(const virtio_mmio_device_t *dev);
void virtio_mmio_interrupt_ack(const virtio_mmio_device_t *dev, u32 bits);
void virtio_memory_barrier(void);
