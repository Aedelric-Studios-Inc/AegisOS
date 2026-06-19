/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ipv4.c */
#include "include/packet.h"

typedef struct {
    u8  version_ihl;
    u8  dscp_ecn;
    u16 total_len;
    u16 id;
    u16 flags_offset;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    u32 src;
    u32 dst;
} __attribute__((packed)) ipv4_hdr_t;

static u32 local_ip;

void ipv4_set_addr(u32 ip) { local_ip = ip; }
u32  ipv4_get_addr(void)   { return local_ip; }

void ipv4_rx(pktbuf_t *pkt) {
    (void)pkt;
    /* Dispatch to TCP/UDP/ICMP — TODO */
}

int ipv4_send(u32 dst, u8 proto, const u8 *payload, u16 len) {
    (void)dst; (void)proto; (void)payload; (void)len;
    return 0;
}
