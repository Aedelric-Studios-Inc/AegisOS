/* SPDX-License-Identifier: Proprietary */
#include "entropy.h"
#include "virtio_mmio.h"
#include "kernel_timer.h"

#define RNG_QUEUE_SIZE 8U
#define RNG_WAIT_SPINS 10000000ULL

static virtio_mmio_device_t *g_rng_dev;
static virtio_queue_t g_rng_q;
static virtq_desc_t g_rng_desc[RNG_QUEUE_SIZE] __attribute__((aligned(4096)));
static virtq_avail_t g_rng_avail __attribute__((aligned(4096)));
static virtq_used_t g_rng_used __attribute__((aligned(4096)));
static u64 g_fallback_state;
static u64 g_generated;
static bool g_strong;

static u64 counter_now(void) {
    u64 v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

static u64 mix64(u64 x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static int rng_fill(void *buf, u32 len) {
    if (!g_rng_dev || !g_rng_dev->driver_ok || !buf || len == 0U) return AEGIS_ENOENT;
    g_rng_desc[0].addr = (u64)(uptr)buf;
    g_rng_desc[0].len = len;
    g_rng_desc[0].flags = VIRTQ_DESC_F_WRITE;
    g_rng_desc[0].next = 0U;
    u16 slot = (u16)(g_rng_avail.idx % g_rng_q.size);
    g_rng_avail.ring[slot] = 0U;
    virtio_memory_barrier();
    g_rng_avail.idx++;
    virtio_mmio_notify(&g_rng_q);
    u64 spins = 0;
    while (virtq_used_load_idx(&g_rng_used) == g_rng_q.last_used_idx) {
        if (++spins >= RNG_WAIT_SPINS) return AEGIS_ETIMEDOUT;
    }
    virtio_memory_barrier();
    u16 used_slot = (u16)(g_rng_q.last_used_idx % g_rng_q.size);
    const virtq_used_elem_t *completed = &g_rng_used.ring[used_slot];
    u32 completed_id = virtq_used_elem_load_id(completed);
    u32 completed_len = virtq_used_elem_load_len(completed);
    g_rng_q.last_used_idx++;
    if (completed_id != 0U) return AEGIS_EIO;
    return completed_len >= len ? AEGIS_OK : AEGIS_EIO;
}

void entropy_init(void) {
    g_rng_dev = NULL;
    g_generated = 0;
    g_strong = false;
    g_fallback_state = mix64(counter_now() ^ (kernel_get_ticks() << 32) ^ (u64)(uptr)&g_fallback_state);
    virtio_mmio_device_t *dev = virtio_mmio_device_by_type(VIRTIO_DEVICE_RNG, 0U);
    if (!dev) return;
    if (virtio_mmio_begin(dev, VIRTIO_F_VERSION_1) != AEGIS_OK) return;
    if (virtio_mmio_setup_queue(dev, &g_rng_q, 0U, RNG_QUEUE_SIZE,
                                g_rng_desc, &g_rng_avail, &g_rng_used) != AEGIS_OK) return;
    virtio_mmio_finish(dev);
    g_rng_dev = dev;
    u64 probe = 0;
    if (rng_fill(&probe, sizeof(probe)) == AEGIS_OK) {
        g_fallback_state ^= mix64(probe);
        g_strong = true;
    }
}

int entropy_get(void *buf, u64 len, bool require_strong) {
    if (!buf || len == 0U) return AEGIS_EINVAL;
    u8 *out = (u8 *)buf;
    u64 off = 0;
    if (g_strong) {
        while (off < len) {
            u32 chunk = (u32)((len - off) > 4096ULL ? 4096ULL : (len - off));
            int rc = rng_fill(out + off, chunk);
            if (rc != AEGIS_OK) {
                g_strong = false;
                break;
            }
            off += chunk;
        }
    }
    if (off < len && require_strong) return AEGIS_EAGAIN;
    while (off < len) {
        g_fallback_state = mix64(g_fallback_state + 0x9e3779b97f4a7c15ULL + counter_now());
        for (u32 i = 0; i < 8U && off < len; i++, off++) out[off] = (u8)(g_fallback_state >> (i * 8U));
    }
    g_generated += len;
    return AEGIS_OK;
}

bool entropy_strong_ready(void) { return g_strong; }
u64 entropy_bytes_generated(void) { return g_generated; }
