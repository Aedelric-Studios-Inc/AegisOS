/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/ipv6.c
 * IPv6 network layer — packet handling, neighbor discovery,
 * and dispatch to transport protocols.
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define IPV6_PROTO_TCP     6
#define IPV6_PROTO_UDP    17
#define IPV6_PROTO_ICMPV6 58

#define IPV6_ADDR_LEN  16
#define ND_MAX_CACHE   64

typedef struct {
    u32 ver_tc_fl;      /* Version (4) + Traffic Class (8) + Flow Label (20) */
    u16 payload_len;
    u8  next_header;
    u8  hop_limit;
    u8  src[16];
    u8  dst[16];
} __attribute__((packed)) ipv6_hdr_t;

/* ICMPv6 header */
typedef struct {
    u8  type;
    u8  code;
    u16 checksum;
} __attribute__((packed)) icmpv6_hdr_t;

/* ICMPv6 types */
#define ICMPV6_ECHO_REQUEST     128
#define ICMPV6_ECHO_REPLY       129
#define ICMPV6_ROUTER_SOL       133
#define ICMPV6_ROUTER_ADV       134
#define ICMPV6_NEIGHBOR_SOL     135
#define ICMPV6_NEIGHBOR_ADV     136

/* Neighbor cache entry */
typedef struct {
    u8  ip[16];
    u8  mac[6];
    u8  state;  /* 0=incomplete, 1=reachable, 2=stale */
    bool valid;
} nd_entry_t;

static nd_entry_t nd_cache[ND_MAX_CACHE];
static u8 local_ipv6[16];
static u8 link_local_ipv6[16];
static bool ipv6_configured = false;

extern void tcp_rx(pktbuf_t *pkt, u32 src_ip);
extern void udp_rx(pktbuf_t *pkt, u32 src_ip);
extern void ethernet_get_local_mac(u8 *mac);
extern int ethernet_tx(const u8 *dst_mac, u16 ethertype, pktbuf_t *pkt);

void ipv6_init(void) {
    for (int i = 0; i < ND_MAX_CACHE; i++) {
        nd_cache[i].valid = false;
    }

    /* Generate link-local address from MAC (EUI-64) */
    u8 mac[6];
    ethernet_get_local_mac(mac);

    link_local_ipv6[0] = 0xFE;
    link_local_ipv6[1] = 0x80;
    for (int i = 2; i < 8; i++) link_local_ipv6[i] = 0;
    link_local_ipv6[8] = mac[0] ^ 0x02;
    link_local_ipv6[9] = mac[1];
    link_local_ipv6[10] = mac[2];
    link_local_ipv6[11] = 0xFF;
    link_local_ipv6[12] = 0xFE;
    link_local_ipv6[13] = mac[3];
    link_local_ipv6[14] = mac[4];
    link_local_ipv6[15] = mac[5];

    ipv6_configured = true;
}

void ipv6_set_addr(const u8 *addr) {
    for (int i = 0; i < 16; i++) local_ipv6[i] = addr[i];
}

static bool ipv6_addr_eq(const u8 *a, const u8 *b) {
    for (int i = 0; i < 16; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool ipv6_is_multicast(const u8 *addr) {
    return addr[0] == 0xFF;
}

static void nd_cache_set(const u8 *ip, const u8 *mac) {
    /* Update existing */
    for (int i = 0; i < ND_MAX_CACHE; i++) {
        if (nd_cache[i].valid && ipv6_addr_eq(nd_cache[i].ip, ip)) {
            for (int j = 0; j < 6; j++) nd_cache[i].mac[j] = mac[j];
            nd_cache[i].state = 1;
            return;
        }
    }
    /* Find empty */
    for (int i = 0; i < ND_MAX_CACHE; i++) {
        if (!nd_cache[i].valid) {
            for (int j = 0; j < 16; j++) nd_cache[i].ip[j] = ip[j];
            for (int j = 0; j < 6; j++) nd_cache[i].mac[j] = mac[j];
            nd_cache[i].state = 1;
            nd_cache[i].valid = true;
            return;
        }
    }
}

static void icmpv6_handle(pktbuf_t *pkt, const u8 *src_addr) {
    if (pkt->len < sizeof(icmpv6_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    icmpv6_hdr_t *hdr = (icmpv6_hdr_t *)(pkt->data + pkt->head);

    switch (hdr->type) {
    case ICMPV6_ECHO_REQUEST: {
        /* Send echo reply */
        pktbuf_t *reply = pktbuf_alloc();
        if (!reply) break;

        /* Copy ICMPv6 payload and change type to reply */
        u16 copy_len = pkt->len < PKT_MAX_LEN ? pkt->len : PKT_MAX_LEN;
        for (u16 i = 0; i < copy_len; i++) {
            reply->data[reply->head + i] = pkt->data[pkt->head + i];
        }
        reply->len = copy_len;

        icmpv6_hdr_t *rep_hdr = (icmpv6_hdr_t *)(reply->data + reply->head);
        rep_hdr->type = ICMPV6_ECHO_REPLY;
        rep_hdr->checksum = 0;
        /* Simplified — full checksum requires pseudo-header */

        /* Build IPv6 header and send */
        /* For now, use link-layer multicast response */
        u8 dst_mac[6] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};
        ethernet_tx(dst_mac, 0x86DD, reply);
        break;
    }
    case ICMPV6_NEIGHBOR_SOL: {
        /* Respond to neighbor solicitation */
        if (pkt->len < sizeof(icmpv6_hdr_t) + 4 + 16) break;
        const u8 *target = pkt->data + pkt->head + sizeof(icmpv6_hdr_t) + 4;
        if (ipv6_addr_eq(target, link_local_ipv6) || ipv6_addr_eq(target, local_ipv6)) {
            /* Send Neighbor Advertisement */
            /* Simplified: just update cache */
            nd_cache_set(src_addr, pkt->data + pkt->head + sizeof(icmpv6_hdr_t) + 4 + 16 + 2);
        }
        break;
    }
    case ICMPV6_NEIGHBOR_ADV: {
        /* Learn neighbor's MAC */
        if (pkt->len >= sizeof(icmpv6_hdr_t) + 4 + 16 + 8) {
            const u8 *target = pkt->data + pkt->head + sizeof(icmpv6_hdr_t) + 4;
            const u8 *mac = pkt->data + pkt->head + sizeof(icmpv6_hdr_t) + 4 + 16 + 2;
            nd_cache_set(target, mac);
        }
        break;
    }
    case ICMPV6_ROUTER_ADV:
        /* Process router advertisement for SLAAC */
        break;
    default:
        break;
    }

    pktbuf_free(pkt);
}

void ipv6_rx(pktbuf_t *pkt) {
    if (!pkt || pkt->len < sizeof(ipv6_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    ipv6_hdr_t *hdr = (ipv6_hdr_t *)(pkt->data + pkt->head);

    /* Verify version */
    u8 version = (u8)((hdr->ver_tc_fl >> 28) & 0xF);
    if (version != 6) { pktbuf_free(pkt); return; }

    /* Check if addressed to us */
    if (!ipv6_addr_eq(hdr->dst, local_ipv6) &&
        !ipv6_addr_eq(hdr->dst, link_local_ipv6) &&
        !ipv6_is_multicast(hdr->dst)) {
        pktbuf_free(pkt);
        return;
    }

    u16 payload_len = (u16)((hdr->payload_len >> 8) | (hdr->payload_len << 8));
    u8 next_header = hdr->next_header;
    u8 src_addr[16];
    for (int i = 0; i < 16; i++) src_addr[i] = hdr->src[i];

    /* Advance past IPv6 header */
    pkt->head += sizeof(ipv6_hdr_t);
    pkt->len = payload_len;

    switch (next_header) {
    case IPV6_PROTO_TCP:
        /* TCP over IPv6 — use link-local scope for now */
        tcp_rx(pkt, 0);
        break;
    case IPV6_PROTO_UDP:
        udp_rx(pkt, 0);
        break;
    case IPV6_PROTO_ICMPV6:
        icmpv6_handle(pkt, src_addr);
        break;
    default:
        pktbuf_free(pkt);
        break;
    }
}
