/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ethernet.c
 * Ethernet frame handling — parsing, construction, and dispatch to
 * upper-layer protocols (IPv4, IPv6, ARP).
 */
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

/* Forward declarations */
extern void ipv4_rx(pktbuf_t *pkt);
extern void ipv6_rx(pktbuf_t *pkt);
extern void arp_rx(pktbuf_t *pkt);

static u8 local_mac[6];
static bool eth_stack_initialized = false;

int net_ethernet_init(void) {
    eth_addr_t addr;
    ethernet_get_mac(&addr);
    for (int i = 0; i < 6; i++) local_mac[i] = addr.mac[i];
    eth_stack_initialized = true;
    return 0;
}

void ethernet_rx(pktbuf_t *pkt) {
    if (!pkt || pkt->len < sizeof(eth_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    eth_hdr_t *hdr = (eth_hdr_t *)pktbuf_pull(pkt, sizeof(eth_hdr_t));
    if (!hdr) { pktbuf_free(pkt); return; }

    u16 et = (u16)((hdr->ethertype >> 8) | (hdr->ethertype << 8)); /* ntoh */

    switch (et) {
    case ETH_TYPE_IPV4:
        ipv4_rx(pkt);
        break;
    case ETH_TYPE_ARP:
        arp_rx(pkt);
        break;
    case ETH_TYPE_IPV6:
        ipv6_rx(pkt);
        break;
    default:
        pktbuf_free(pkt);
        break;
    }
}

int ethernet_tx(const u8 *dst_mac, u16 ethertype, pktbuf_t *pkt) {
    if (!pkt || !dst_mac) return -1;

    /* Prepend Ethernet header */
    u8 *hdr_data = pktbuf_push(pkt, sizeof(eth_hdr_t));
    if (!hdr_data) { pktbuf_free(pkt); return -1; }

    eth_hdr_t *hdr = (eth_hdr_t *)hdr_data;
    for (int i = 0; i < 6; i++) {
        hdr->dst[i] = dst_mac[i];
        hdr->src[i] = local_mac[i];
    }
    hdr->ethertype = (u16)((ethertype >> 8) | (ethertype << 8)); /* hton */

    /* Send via HAL */
    int ret = ethernet_send(pkt->data + pkt->head, pkt->len);
    pktbuf_free(pkt);
    return ret;
}

void ethernet_get_local_mac(u8 *mac) {
    for (int i = 0; i < 6; i++) mac[i] = local_mac[i];
}
