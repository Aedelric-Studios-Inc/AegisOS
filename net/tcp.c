/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/tcp.c */
#include "include/packet.h"

typedef struct {
    u16 src_port; u16 dst_port;
    u32 seq; u32 ack;
    u8  data_offset; u8 flags;
    u16 window; u16 checksum; u16 urgent;
} __attribute__((packed)) tcp_hdr_t;

void tcp_rx(pktbuf_t *pkt, u32 src_ip) { (void)pkt; (void)src_ip; }
