/* SPDX-License-Identifier: Proprietary */
#include "virtio_blk.h"
#include "virtio_mmio.h"
#include "block_storage.h"

#define VIRTIO_BLK_T_IN     0U
#define VIRTIO_BLK_T_OUT    1U
#define VIRTIO_BLK_T_FLUSH  4U
#define VIRTIO_BLK_S_OK     0U
#define VIRTIO_BLK_QUEUE_SIZE 8U
#define VIRTIO_BLK_MAX_SECTORS 8U
#define VIRTIO_BLK_CONFIG_CAPACITY 0x100U
#define VIRTIO_BLK_F_FLUSH          (1ULL << 9)

typedef struct virtio_blk_req {
    u32 type;
    u32 reserved;
    u64 sector;
} __attribute__((packed)) virtio_blk_req_t;

typedef struct virtio_blk_queue_mem {
    virtq_desc_t desc[VIRTIO_BLK_QUEUE_SIZE];
    virtq_avail_t avail;
    virtq_used_t used;
} __attribute__((aligned(4096))) virtio_blk_queue_mem_t;

static virtio_mmio_device_t *g_dev;
static virtio_queue_t g_queue;
static virtio_blk_queue_mem_t g_qmem;
static virtio_blk_req_t g_req __attribute__((aligned(16)));
static u8 g_data[VIRTIO_BLK_MAX_SECTORS * 512U] __attribute__((aligned(16)));
static u8 g_status __attribute__((aligned(16)));
static u64 g_capacity;
static bool g_ready;

static volatile u32 *reg32(u64 base, u32 off) { return (volatile u32 *)(uptr)(base + off); }
static u32 read32(u64 base, u32 off) { return *reg32(base, off); }
static void copy_bytes(void *dst, const void *src, u64 len) {
    u8 *d = (u8 *)dst; const u8 *s = (const u8 *)src; for (u64 i = 0; i < len; i++) d[i] = s[i];
}

static int submit(u32 type, u64 sector, u32 sectors, const void *write_src, void *read_dst) {
    if (!g_ready || sectors > VIRTIO_BLK_MAX_SECTORS) return AEGIS_EINVAL;
    u32 bytes = sectors * 512U;
    g_req.type = type;
    g_req.reserved = 0U;
    g_req.sector = sector;
    g_status = 0xffU;
    if (type == VIRTIO_BLK_T_OUT && bytes) copy_bytes(g_data, write_src, bytes);

    g_queue.desc[0].addr = (u64)(uptr)&g_req;
    g_queue.desc[0].len = sizeof(g_req);
    g_queue.desc[0].flags = VIRTQ_DESC_F_NEXT;
    g_queue.desc[0].next = 1U;
    g_queue.desc[1].addr = (u64)(uptr)g_data;
    g_queue.desc[1].len = bytes;
    g_queue.desc[1].flags = VIRTQ_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0U);
    g_queue.desc[1].next = 2U;
    g_queue.desc[2].addr = (u64)(uptr)&g_status;
    g_queue.desc[2].len = 1U;
    g_queue.desc[2].flags = VIRTQ_DESC_F_WRITE;
    g_queue.desc[2].next = 0U;
    if (type == VIRTIO_BLK_T_FLUSH) {
        g_queue.desc[0].next = 2U;
    }

    u16 avail_idx = g_queue.avail->idx;
    g_queue.avail->ring[avail_idx % g_queue.size] = 0U;
    virtio_memory_barrier();
    g_queue.avail->idx = (u16)(avail_idx + 1U);
    virtio_mmio_notify(&g_queue);

    u32 spins = 0U;
    while (virtq_used_load_idx(g_queue.used) == g_queue.last_used_idx) {
        __asm__ volatile("yield");
        if (++spins > 100000000U) return AEGIS_ETIMEDOUT;
    }
    virtio_memory_barrier();
    g_queue.last_used_idx = virtq_used_load_idx(g_queue.used);
    if (g_status != VIRTIO_BLK_S_OK) return AEGIS_EIO;
    if (type == VIRTIO_BLK_T_IN && bytes) copy_bytes(read_dst, g_data, bytes);
    return AEGIS_OK;
}

int virtio_blk_read_sectors(u64 sector, u32 count, void *buffer) {
    if (!buffer || count == 0U || sector >= g_capacity || count > g_capacity - sector) return AEGIS_EINVAL;
    u8 *out = (u8 *)buffer;
    while (count) {
        u32 chunk = count > VIRTIO_BLK_MAX_SECTORS ? VIRTIO_BLK_MAX_SECTORS : count;
        int rc = submit(VIRTIO_BLK_T_IN, sector, chunk, NULL, out);
        if (rc != AEGIS_OK) return rc;
        sector += chunk; count -= chunk; out += chunk * 512U;
    }
    return AEGIS_OK;
}
int virtio_blk_write_sectors(u64 sector, u32 count, const void *buffer) {
    if (!buffer || count == 0U || sector >= g_capacity || count > g_capacity - sector) return AEGIS_EINVAL;
    const u8 *in = (const u8 *)buffer;
    while (count) {
        u32 chunk = count > VIRTIO_BLK_MAX_SECTORS ? VIRTIO_BLK_MAX_SECTORS : count;
        int rc = submit(VIRTIO_BLK_T_OUT, sector, chunk, in, NULL);
        if (rc != AEGIS_OK) return rc;
        sector += chunk; count -= chunk; in += chunk * 512U;
    }
    return AEGIS_OK;
}
int virtio_blk_flush(void) { return submit(VIRTIO_BLK_T_FLUSH, 0U, 0U, NULL, NULL); }

static int backend_read(void *ctx, u64 lba, u32 count, void *buffer) { (void)ctx; return virtio_blk_read_sectors(lba, count, buffer); }
static int backend_write(void *ctx, u64 lba, u32 count, const void *buffer) { (void)ctx; return virtio_blk_write_sectors(lba, count, buffer); }
static int backend_flush(void *ctx) { (void)ctx; return virtio_blk_flush(); }
static const aegis_block_ops_t g_ops = { backend_read, backend_write, backend_flush };

int virtio_blk_init(void) {
    g_ready = false;
    g_dev = virtio_mmio_device_by_type(VIRTIO_DEVICE_BLOCK, 0U);
    if (!g_dev) return AEGIS_ENOENT;
    int rc = virtio_mmio_begin(g_dev, VIRTIO_F_VERSION_1 | VIRTIO_BLK_F_FLUSH);
    if (rc != AEGIS_OK) return rc;
    rc = virtio_mmio_setup_queue(g_dev, &g_queue, 0U, VIRTIO_BLK_QUEUE_SIZE,
                                 g_qmem.desc, &g_qmem.avail, &g_qmem.used);
    if (rc != AEGIS_OK) return rc;
    u32 low = read32(g_dev->base, VIRTIO_BLK_CONFIG_CAPACITY);
    u32 high = read32(g_dev->base, VIRTIO_BLK_CONFIG_CAPACITY + 4U);
    g_capacity = (u64)low | ((u64)high << 32);
    if (g_capacity == 0U) return AEGIS_EIO;
    virtio_mmio_finish(g_dev);
    g_ready = true;
    u32 id = 0U;
    rc = block_storage_register_device("virtio-blk0", 512U, g_capacity,
                                       AEGIS_BLOCK_FLAG_WRITABLE | AEGIS_BLOCK_FLAG_VIRTIO,
                                       &g_ops, NULL, &id);
    return rc;
}
bool virtio_blk_ready(void) { return g_ready; }
u64 virtio_blk_capacity_sectors(void) { return g_capacity; }
