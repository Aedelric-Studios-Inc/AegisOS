/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/packet.c
 * Fixed-size packet buffer pool for early kernel/router bring-up.
 *
 * This intentionally avoids heap allocation so the network stack can be
 * brought up before the general kernel allocator is trusted.
 */
#include "include/packet.h"

#define PKT_POOL_SIZE 64

static pktbuf_t pkt_pool[PKT_POOL_SIZE];
static pktbuf_t *free_list;
static bool packet_pool_initialized;

static void pktbuf_pool_init(void) {
    free_list = NULL;
    for (u32 i = 0; i < PKT_POOL_SIZE; i++) {
        pkt_pool[i].next = free_list;
        free_list = &pkt_pool[i];
    }
    packet_pool_initialized = true;
}

pktbuf_t *pktbuf_alloc(void) {
    if (!packet_pool_initialized) {
        pktbuf_pool_init();
    }

    if (!free_list) {
        return NULL;
    }

    pktbuf_t *pkt = free_list;
    free_list = free_list->next;
    pkt->len = 0;
    pkt->head = PKT_MAX_LEN / 2;
    pkt->next = NULL;
    return pkt;
}

void pktbuf_free(pktbuf_t *pkt) {
    if (!pkt) {
        return;
    }

    pkt->len = 0;
    pkt->head = 0;
    pkt->next = free_list;
    free_list = pkt;
}

u8 *pktbuf_push(pktbuf_t *pkt, u16 bytes) {
    if (!pkt || bytes > pkt->head) {
        return NULL;
    }

    pkt->head = (u16)(pkt->head - bytes);
    pkt->len = (u16)(pkt->len + bytes);
    return pkt->data + pkt->head;
}

u8 *pktbuf_pull(pktbuf_t *pkt, u16 bytes) {
    if (!pkt || bytes > pkt->len) {
        return NULL;
    }

    u8 *ptr = pkt->data + pkt->head;
    pkt->head = (u16)(pkt->head + bytes);
    pkt->len = (u16)(pkt->len - bytes);
    return ptr;
}
