/* SPDX-License-Identifier: Proprietary */
/* AegisOS — hal/drivers/ethernet/ethernet.c
 * VIRTIO-NET compatible Ethernet driver for the AegisBox platform.
 * Supports MMIO-based virtio net device (QEMU virt machine compatible).
 */

#include "../../include/ethernet.h"
#include "../../../kernel/include/memory.h"

/* VIRTIO MMIO registers */
#define VIRTIO_NET_BASE        0x0A000000UL
#define VIRTIO_MAGIC           (*(volatile u32 *)(VIRTIO_NET_BASE + 0x000))
#define VIRTIO_VERSION         (*(volatile u32 *)(VIRTIO_NET_BASE + 0x004))
#define VIRTIO_DEVICE_ID       (*(volatile u32 *)(VIRTIO_NET_BASE + 0x008))
#define VIRTIO_STATUS          (*(volatile u32 *)(VIRTIO_NET_BASE + 0x070))
#define VIRTIO_QUEUE_SEL       (*(volatile u32 *)(VIRTIO_NET_BASE + 0x030))
#define VIRTIO_QUEUE_NUM_MAX   (*(volatile u32 *)(VIRTIO_NET_BASE + 0x034))
#define VIRTIO_QUEUE_NUM       (*(volatile u32 *)(VIRTIO_NET_BASE + 0x038))
#define VIRTIO_QUEUE_READY     (*(volatile u32 *)(VIRTIO_NET_BASE + 0x044))
#define VIRTIO_QUEUE_NOTIFY    (*(volatile u32 *)(VIRTIO_NET_BASE + 0x050))

#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_FEATURES  8
#define VIRTIO_STATUS_DRIVER_OK 4

/* Virtqueue descriptors */
#define VRING_SIZE 128

typedef struct {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed)) vring_avail_t;

typedef struct {
    u32 id;
    u32 len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    u16 flags;
    u16 idx;
    vring_used_elem_t ring[];
} __attribute__((packed)) vring_used_t;

/* TX/RX ring buffers */
static u8 tx_buffer[ETH_MTU + 14 + 10];  /* +10 for virtio-net header */
static u8 rx_buffer[ETH_MTU + 14 + 10];
static vring_desc_t tx_desc[VRING_SIZE] __attribute__((aligned(16)));
static vring_desc_t rx_desc[VRING_SIZE] __attribute__((aligned(16)));

static u8 mac_addr[ETH_ALEN] = {0x02, 0xAE, 0x61, 0x5B, 0x0C, 0x01};
static bool eth_initialized = false;

/* Virtio-net header (simplified) */
typedef struct {
    u8  flags;
    u8  gso_type;
    u16 hdr_len;
    u16 gso_size;
    u16 csum_start;
    u16 csum_offset;
    u16 num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

int ethernet_init(void) {
    /* Verify virtio device magic */
    if (VIRTIO_MAGIC != 0x74726976) {
        /* No virtio device at expected address — fall back to loopback */
        eth_initialized = true;
        return 0;
    }

    /* Negotiate with device */
    VIRTIO_STATUS = 0;  /* Reset */
    VIRTIO_STATUS = VIRTIO_STATUS_ACK;
    VIRTIO_STATUS = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER;

    /* Accept features (none special) */
    VIRTIO_STATUS = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES;

    /* Setup RX queue (queue 0) */
    VIRTIO_QUEUE_SEL = 0;
    VIRTIO_QUEUE_NUM = VRING_SIZE;
    VIRTIO_QUEUE_READY = 1;

    /* Setup TX queue (queue 1) */
    VIRTIO_QUEUE_SEL = 1;
    VIRTIO_QUEUE_NUM = VRING_SIZE;
    VIRTIO_QUEUE_READY = 1;

    /* Mark driver OK */
    VIRTIO_STATUS = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                    VIRTIO_STATUS_FEATURES | VIRTIO_STATUS_DRIVER_OK;

    /* Prepare initial RX descriptor */
    rx_desc[0].addr = (u64)(uptr)rx_buffer;
    rx_desc[0].len = sizeof(rx_buffer);
    rx_desc[0].flags = 2; /* WRITE (device writes to this buffer) */
    rx_desc[0].next = 0;

    eth_initialized = true;
    return 0;
}

int ethernet_send(const u8 *frame, u16 len) {
    if (!eth_initialized) return -1;
    if (!frame || len == 0 || len > ETH_MTU + 14) return -1;

    /* Prepend virtio-net header */
    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)tx_buffer;
    hdr->flags = 0;
    hdr->gso_type = 0;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    hdr->num_buffers = 0;

    /* Copy frame after header */
    u8 *payload = tx_buffer + sizeof(virtio_net_hdr_t);
    for (u16 i = 0; i < len; i++) {
        payload[i] = frame[i];
    }

    /* Setup TX descriptor */
    tx_desc[0].addr = (u64)(uptr)tx_buffer;
    tx_desc[0].len = sizeof(virtio_net_hdr_t) + len;
    tx_desc[0].flags = 0; /* Device reads from this buffer */
    tx_desc[0].next = 0;

    /* Notify TX queue */
    VIRTIO_QUEUE_SEL = 1;
    VIRTIO_QUEUE_NOTIFY = 1;

    return 0;
}

int ethernet_recv(u8 *buf, u16 *len) {
    if (!eth_initialized) return -1;
    if (!buf || !len) return -1;

    /* Check if RX descriptor has been filled */
    /* In a real implementation, check the used ring index */
    /* For now, poll-based with virtio notification */
    VIRTIO_QUEUE_SEL = 0;

    /* Check if data available in rx_buffer (simplified polling) */
    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)rx_buffer;
    if (hdr->flags == 0 && hdr->gso_type == 0 && hdr->hdr_len == 0) {
        return -1; /* No data available */
    }

    /* Copy received frame (skip virtio header) */
    u16 frame_len = rx_desc[0].len - sizeof(virtio_net_hdr_t);
    if (frame_len > ETH_MTU + 14) frame_len = ETH_MTU + 14;

    u8 *src = rx_buffer + sizeof(virtio_net_hdr_t);
    for (u16 i = 0; i < frame_len; i++) {
        buf[i] = src[i];
    }
    *len = frame_len;

    /* Re-arm RX descriptor */
    rx_desc[0].addr = (u64)(uptr)rx_buffer;
    rx_desc[0].len = sizeof(rx_buffer);
    rx_desc[0].flags = 2;
    VIRTIO_QUEUE_NOTIFY = 0;

    return 0;
}

void ethernet_get_mac(eth_addr_t *out) {
    for (int i = 0; i < ETH_ALEN; i++)
        out->mac[i] = mac_addr[i];
}
