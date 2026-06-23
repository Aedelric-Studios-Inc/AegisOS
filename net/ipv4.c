/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ipv4.c
 * IPv4 network layer — packet parsing, routing, fragmentation,
 * and dispatch to transport protocols (TCP, UDP, ICMP).
 */
#include "include/packet.h"
#include "include/routing.h"
#include "include/firewall.h"
#include "../hal/include/ethernet.h"

#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP   17

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
static u32 local_netmask = 0xFFFFFF00; /* /24 default */
static u32 local_gateway = 0;
static u16 ip_id_counter = 0;

/* Forward declarations */
extern void tcp_rx(pktbuf_t *pkt, u32 src_ip);
extern void udp_rx(pktbuf_t *pkt, u32 src_ip);
extern void icmp_rx(pktbuf_t *pkt, u32 src_ip);
extern const u8 *arp_cache_lookup(u32 ip);
extern int arp_send_request(u32 target_ip);
extern int ethernet_tx(const u8 *dst_mac, u16 ethertype, pktbuf_t *pkt);

static u16 ip_checksum(const void *data, u16 len) {
    const u16 *ptr = (const u16 *)data;
    u32 sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const u8 *)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (u16)~sum;
}

void ipv4_set_addr(u32 ip) { local_ip = ip; }
u32  ipv4_get_addr(void)   { return local_ip; }
void ipv4_set_netmask(u32 mask) { local_netmask = mask; }
void ipv4_set_gateway(u32 gw) { local_gateway = gw; }

void ipv4_rx(pktbuf_t *pkt) {
    if (!pkt || pkt->len < sizeof(ipv4_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    ipv4_hdr_t *hdr = (ipv4_hdr_t *)(pkt->data + pkt->head);

    /* Validate version */
    u8 version = (hdr->version_ihl >> 4) & 0xF;
    if (version != 4) { pktbuf_free(pkt); return; }

    /* Header length in bytes */
    u8 ihl = (hdr->version_ihl & 0xF) * 4;
    if (ihl < 20 || pkt->len < ihl) { pktbuf_free(pkt); return; }

    /* Verify checksum */
    u16 saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    u16 calc_checksum = ip_checksum(hdr, ihl);
    hdr->checksum = saved_checksum;
    if (calc_checksum != saved_checksum) {
        pktbuf_free(pkt);
        return;
    }

    /* Check destination — accept if addressed to us or broadcast */
    u32 dst = hdr->dst;
    if (dst != local_ip && dst != 0xFFFFFFFF &&
        (dst & ~local_netmask) != (~local_netmask & 0xFFFFFFFF)) {
        /* Not for us — could forward if routing enabled */
        pktbuf_free(pkt);
        return;
    }

    /* Firewall check */
    if (firewall_check(pkt) == FW_DROP) {
        pktbuf_free(pkt);
        return;
    }

    /* Extract protocol and advance past IP header */
    u8 protocol = hdr->protocol;
    u32 src_ip = hdr->src;
    pkt->head += ihl;
    pkt->len -= ihl;

    /* Dispatch to transport layer */
    switch (protocol) {
    case IP_PROTO_ICMP:
        icmp_rx(pkt, src_ip);
        break;
    case IP_PROTO_TCP:
        tcp_rx(pkt, src_ip);
        break;
    case IP_PROTO_UDP:
        udp_rx(pkt, src_ip);
        break;
    default:
        pktbuf_free(pkt);
        break;
    }
}

int ipv4_send(u32 dst, u8 proto, const u8 *payload, u16 len) {
    if (!payload || len == 0) return -1;

    pktbuf_t *pkt = pktbuf_alloc();
    if (!pkt) return -1;

    /* Reserve space for Ethernet header (handled by ethernet_tx) */
    u16 total_len = 20 + len; /* IP header + payload */

    /* Build IP header */
    ipv4_hdr_t *hdr = (ipv4_hdr_t *)(pkt->data + pkt->head);
    hdr->version_ihl = 0x45; /* IPv4, IHL=5 (20 bytes) */
    hdr->dscp_ecn = 0;
    hdr->total_len = (u16)((total_len >> 8) | (total_len << 8));
    hdr->id = (u16)((ip_id_counter >> 8) | (ip_id_counter << 8));
    ip_id_counter++;
    hdr->flags_offset = 0x0040; /* Don't Fragment, network byte order */
    hdr->ttl = 64;
    hdr->protocol = proto;
    hdr->checksum = 0;
    hdr->src = local_ip;
    hdr->dst = dst;

    /* Compute header checksum */
    hdr->checksum = ip_checksum(hdr, 20);

    /* Copy payload */
    u8 *payload_dst = (u8 *)hdr + 20;
    for (u16 i = 0; i < len; i++) {
        payload_dst[i] = payload[i];
    }
    pkt->len = total_len;

    /* Determine next hop */
    u32 next_hop;
    if ((dst & local_netmask) == (local_ip & local_netmask)) {
        next_hop = dst; /* Same subnet — direct delivery */
    } else {
        /* Use routing table or default gateway */
        route_t *route = routing_lookup(dst);
        if (route && route->gateway) {
            next_hop = route->gateway;
        } else {
            next_hop = local_gateway;
        }
    }

    if (next_hop == 0) {
        pktbuf_free(pkt);
        return -1; /* No route to host */
    }

    /* ARP resolve next hop */
    const u8 *dst_mac = arp_cache_lookup(next_hop);
    if (!dst_mac) {
        arp_send_request(next_hop);
        pktbuf_free(pkt);
        return -1; /* ARP pending — caller should retry */
    }

    return ethernet_tx(dst_mac, 0x0800, pkt);
}
