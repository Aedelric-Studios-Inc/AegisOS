/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ethernet.c */
#include "include/packet.h"
#include "../hal/include/ethernet.h"

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV6 0x86DD

typedef struct {
    u8  dst[6];
    u8  src[6];
    u16 ethertype;
} __attribute__((packed)) eth_hdr_t;

void ethernet_rx(pktbuf_t *pkt) {
    if (pkt->len < sizeof(eth_hdr_t)) { pktbuf_free(pkt); return; }
    eth_hdr_t *hdr = (eth_hdr_t *)pkt->data;
    u16 et = (u16)((hdr->ethertype >> 8) | (hdr->ethertype << 8));
    switch (et) {
    case ETH_TYPE_IPV4: /* ipv4_rx(pkt); */ break;
    case ETH_TYPE_ARP:  /* arp_rx(pkt);  */ break;
    case ETH_TYPE_IPV6: /* ipv6_rx(pkt); */ break;
    default: pktbuf_free(pkt); break;
    }
}
