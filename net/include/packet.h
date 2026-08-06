/* SPDX-License-Identifier: Proprietary */
#pragma once
/* net/include/packet.h — Network packet buffer */

#include "../../kernel/include/types.h"

#define PKT_MAX_LEN  1514

typedef struct pktbuf {
    u16  len;
    u16  head;
    u8   data[PKT_MAX_LEN];
    struct pktbuf *next;
} pktbuf_t;

pktbuf_t *pktbuf_alloc(void);
void      pktbuf_free(pktbuf_t *pkt);
u8       *pktbuf_push(pktbuf_t *pkt, u16 bytes);
u8       *pktbuf_pull(pktbuf_t *pkt, u16 bytes);
