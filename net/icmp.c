/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/icmp.c */
#include "include/packet.h"

typedef struct { u8 type; u8 code; u16 checksum; u32 rest; } __attribute__((packed)) icmp_hdr_t;

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

void icmp_rx(pktbuf_t *pkt, u32 src_ip) {
    (void)pkt; (void)src_ip;
    /* Echo reply — TODO */
}
