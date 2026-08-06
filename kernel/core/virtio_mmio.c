/* SPDX-License-Identifier: Proprietary */
#include "virtio_mmio.h"
#include "device_tree.h"

#define VMMIO_MAGIC             0x000U
#define VMMIO_VERSION           0x004U
#define VMMIO_DEVICE_ID         0x008U
#define VMMIO_VENDOR_ID         0x00cU
#define VMMIO_DEVICE_FEATURES   0x010U
#define VMMIO_DEVICE_FEATURES_SEL 0x014U
#define VMMIO_DRIVER_FEATURES   0x020U
#define VMMIO_DRIVER_FEATURES_SEL 0x024U
#define VMMIO_QUEUE_SEL         0x030U
#define VMMIO_QUEUE_NUM_MAX     0x034U
#define VMMIO_QUEUE_NUM         0x038U
#define VMMIO_QUEUE_READY       0x044U
#define VMMIO_QUEUE_NOTIFY      0x050U
#define VMMIO_INTERRUPT_STATUS  0x060U
#define VMMIO_INTERRUPT_ACK     0x064U
#define VMMIO_STATUS            0x070U
#define VMMIO_QUEUE_DESC_LOW    0x080U
#define VMMIO_QUEUE_DESC_HIGH   0x084U
#define VMMIO_QUEUE_AVAIL_LOW   0x090U
#define VMMIO_QUEUE_AVAIL_HIGH  0x094U
#define VMMIO_QUEUE_USED_LOW    0x0a0U
#define VMMIO_QUEUE_USED_HIGH   0x0a4U

#define VIRTIO_STATUS_ACKNOWLEDGE 1U
#define VIRTIO_STATUS_DRIVER      2U
#define VIRTIO_STATUS_DRIVER_OK   4U
#define VIRTIO_STATUS_FEATURES_OK 8U
#define VIRTIO_STATUS_FAILED      128U

static virtio_mmio_device_t g_devices[VIRTIO_MMIO_MAX_DEVICES];
static u32 g_device_count;

static volatile u32 *reg32(u64 base, u32 off) {
    return (volatile u32 *)(uptr)(base + off);
}
static u32 read32(u64 base, u32 off) { return *reg32(base, off); }
static void write32(u64 base, u32 off, u32 value) { *reg32(base, off) = value; }

void virtio_memory_barrier(void) { __asm__ volatile("dmb ish" ::: "memory"); }

static void zero_bytes(void *ptr, u64 len) {
    u8 *p = (u8 *)ptr;
    for (u64 i = 0; i < len; i++) p[i] = 0;
}

void virtio_mmio_bus_init(void) {
    zero_bytes(g_devices, sizeof(g_devices));
    g_device_count = 0;
}

static void probe_base(u64 base, u32 irq) {
    if (g_device_count >= VIRTIO_MMIO_MAX_DEVICES) return;
    if (read32(base, VMMIO_MAGIC) != VIRTIO_MMIO_MAGIC_VALUE) return;
    u32 id = read32(base, VMMIO_DEVICE_ID);
    if (id == 0U) return;
    virtio_mmio_device_t *dev = &g_devices[g_device_count++];
    dev->base = base;
    dev->irq = irq;
    dev->version = read32(base, VMMIO_VERSION);
    dev->device_id = id;
    dev->vendor_id = read32(base, VMMIO_VENDOR_ID);
    dev->present = true;
}

static void probe_qemu_virt_window(void) {
    /* QEMU virt exposes 32 virtio-mmio transports in this fixed window. */
    for (u32 i = 0; i < 32U && g_device_count < VIRTIO_MMIO_MAX_DEVICES; i++) {
        probe_base(0x0a000000ULL + (u64)i * 0x200ULL, 48U + i);
    }
}

int virtio_mmio_discover(void) {
    virtio_mmio_bus_init();
    const aegis_dtb_platform_t *plat = device_tree_platform();
    if (plat && plat->valid && plat->virtio_count) {
        for (u32 i = 0; i < plat->virtio_count; i++) {
            probe_base(plat->virtio[i].base, plat->virtio[i].irq);
        }
    }

    /* A valid FDT can still contain unusable candidate addresses when a
     * parent-bus ranges translation is absent or unsupported.  Do not let
     * such candidates suppress the already-proven QEMU virt discovery path.
     * Probe the canonical window whenever no live transport was found. */
    if (g_device_count == 0U) probe_qemu_virt_window();

    return g_device_count ? AEGIS_OK : AEGIS_ENOENT;
}

u32 virtio_mmio_device_count(void) { return g_device_count; }
virtio_mmio_device_t *virtio_mmio_device_by_type(u32 device_id, u32 ordinal) {
    for (u32 i = 0; i < g_device_count; i++) {
        if (g_devices[i].device_id != device_id) continue;
        if (ordinal == 0U) return &g_devices[i];
        ordinal--;
    }
    return NULL;
}

int virtio_mmio_begin(virtio_mmio_device_t *dev, u64 accepted_features) {
    if (!dev || !dev->present || dev->version != 2U) return AEGIS_EINVAL;
    write32(dev->base, VMMIO_STATUS, 0U);
    write32(dev->base, VMMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    write32(dev->base, VMMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    write32(dev->base, VMMIO_DEVICE_FEATURES_SEL, 0U);
    u64 offered = read32(dev->base, VMMIO_DEVICE_FEATURES);
    write32(dev->base, VMMIO_DEVICE_FEATURES_SEL, 1U);
    offered |= (u64)read32(dev->base, VMMIO_DEVICE_FEATURES) << 32;
    u64 selected = offered & accepted_features;
    write32(dev->base, VMMIO_DRIVER_FEATURES_SEL, 0U);
    write32(dev->base, VMMIO_DRIVER_FEATURES, (u32)selected);
    write32(dev->base, VMMIO_DRIVER_FEATURES_SEL, 1U);
    write32(dev->base, VMMIO_DRIVER_FEATURES, (u32)(selected >> 32));

    u32 status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    write32(dev->base, VMMIO_STATUS, status);
    if ((read32(dev->base, VMMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0U) {
        write32(dev->base, VMMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return AEGIS_EINVAL;
    }
    return AEGIS_OK;
}

int virtio_mmio_setup_queue(virtio_mmio_device_t *dev, virtio_queue_t *q,
                             u16 index, u16 requested_size,
                             virtq_desc_t *desc, virtq_avail_t *avail,
                             virtq_used_t *used) {
    if (!dev || !q || !desc || !avail || !used || requested_size == 0U || requested_size > 256U) return AEGIS_EINVAL;
    write32(dev->base, VMMIO_QUEUE_SEL, index);
    u32 max = read32(dev->base, VMMIO_QUEUE_NUM_MAX);
    if (max == 0U) return AEGIS_ENOENT;
    u16 size = requested_size < max ? requested_size : (u16)max;
    if (read32(dev->base, VMMIO_QUEUE_READY) != 0U) return AEGIS_EBUSY;
    zero_bytes(desc, sizeof(virtq_desc_t) * size);
    zero_bytes(avail, sizeof(*avail));
    zero_bytes(used, sizeof(*used));
    write32(dev->base, VMMIO_QUEUE_NUM, size);
    u64 desc_addr = (u64)(uptr)desc;
    u64 avail_addr = (u64)(uptr)avail;
    u64 used_addr = (u64)(uptr)used;
    write32(dev->base, VMMIO_QUEUE_DESC_LOW, (u32)desc_addr);
    write32(dev->base, VMMIO_QUEUE_DESC_HIGH, (u32)(desc_addr >> 32));
    write32(dev->base, VMMIO_QUEUE_AVAIL_LOW, (u32)avail_addr);
    write32(dev->base, VMMIO_QUEUE_AVAIL_HIGH, (u32)(avail_addr >> 32));
    write32(dev->base, VMMIO_QUEUE_USED_LOW, (u32)used_addr);
    write32(dev->base, VMMIO_QUEUE_USED_HIGH, (u32)(used_addr >> 32));
    write32(dev->base, VMMIO_QUEUE_READY, 1U);
    q->device = dev;
    q->index = index;
    q->size = size;
    q->last_used_idx = 0U;
    q->desc = desc;
    q->avail = avail;
    q->used = used;
    return AEGIS_OK;
}

void virtio_mmio_finish(virtio_mmio_device_t *dev) {
    if (!dev) return;
    u32 status = read32(dev->base, VMMIO_STATUS);
    write32(dev->base, VMMIO_STATUS, status | VIRTIO_STATUS_DRIVER_OK);
    dev->driver_ok = true;
}
void virtio_mmio_notify(const virtio_queue_t *q) {
    if (!q || !q->device) return;
    virtio_memory_barrier();
    write32(q->device->base, VMMIO_QUEUE_NOTIFY, q->index);
}
u32 virtio_mmio_interrupt_status(const virtio_mmio_device_t *dev) {
    return dev ? read32(dev->base, VMMIO_INTERRUPT_STATUS) : 0U;
}
void virtio_mmio_interrupt_ack(const virtio_mmio_device_t *dev, u32 bits) {
    if (dev) write32(dev->base, VMMIO_INTERRUPT_ACK, bits);
}
