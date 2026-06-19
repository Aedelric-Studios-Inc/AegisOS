/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/udp.c */
#include "include/packet.h"

typedef struct { u16 src_port; u16 dst_port; u16 length; u16 checksum; } __attribute__((packed)) udp_hdr_t;

void udp_rx(pktbuf_t *pkt, u32 src_ip) { (void)pkt; (void)src_ip; }
int  udp_send(u32 dst_ip, u16 dst_port, u16 src_port, const u8 *data, u16 len) {
    (void)dst_ip; (void)dst_port; (void)src_port; (void)data; (void)len;
    return 0;
}
