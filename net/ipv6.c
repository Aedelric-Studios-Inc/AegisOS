/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ipv6.c */
#include "include/packet.h"

typedef struct {
    u32 ver_tc_fl;
    u16 payload_len;
    u8  next_header;
    u8  hop_limit;
    u8  src[16];
    u8  dst[16];
} __attribute__((packed)) ipv6_hdr_t;

void ipv6_rx(pktbuf_t *pkt) {
    (void)pkt;
    /* IPv6 dispatch — TODO */
}
