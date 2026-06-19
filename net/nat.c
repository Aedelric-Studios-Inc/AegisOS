/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/nat.c */
#include "include/packet.h"

int nat_translate_outbound(pktbuf_t *pkt, u32 wan_ip) { (void)pkt; (void)wan_ip; return 0; }
int nat_translate_inbound(pktbuf_t *pkt)              { (void)pkt; return 0; }
