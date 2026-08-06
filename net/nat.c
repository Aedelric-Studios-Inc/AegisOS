/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/nat.c
 * Network Address Translation — masquerade outbound traffic behind
 * the WAN IP and track state for inbound translation. Supports
 * port-based NAPT (Network Address Port Translation).
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define NAT_MAX_ENTRIES  4096
#define NAT_PORT_MIN     10000
#define NAT_PORT_MAX     60000
#define NAT_TIMEOUT      300  /* Seconds */

typedef struct {
    u32 internal_ip;
    u16 internal_port;
    u32 external_ip;
    u16 external_port;
    u32 remote_ip;
    u16 remote_port;
    u8  protocol;
    u32 last_seen;
    bool active;
} nat_entry_t;

/* IPv4 header for rewriting */
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
} __attribute__((packed)) nat_ipv4_hdr_t;

static nat_entry_t nat_table[NAT_MAX_ENTRIES];
static u16 next_nat_port = NAT_PORT_MIN;
static bool nat_enabled = false;
static u32 wan_ip_addr = 0;

void nat_init(void) {
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        nat_table[i].active = false;
    }
    nat_enabled = false;
}

void nat_enable(u32 wan_ip) {
    wan_ip_addr = wan_ip;
    nat_enabled = true;
}

void nat_disable(void) {
    nat_enabled = false;
}

static u16 alloc_nat_port(void) {
    u16 port = next_nat_port++;
    if (next_nat_port > NAT_PORT_MAX) next_nat_port = NAT_PORT_MIN;
    return port;
}

static void ip_recalc_checksum(nat_ipv4_hdr_t *hdr) {
    hdr->checksum = 0;
    u32 sum = 0;
    const u16 *p = (const u16 *)hdr;
    u8 ihl = (hdr->version_ihl & 0xF) * 4;
    for (int i = 0; i < ihl / 2; i++) sum += p[i];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    hdr->checksum = (u16)~sum;
}

static nat_entry_t *nat_find_outbound(u32 src_ip, u16 src_port, u32 dst_ip,
                                       u16 dst_port, u8 proto) {
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        if (nat_table[i].active &&
            nat_table[i].internal_ip == src_ip &&
            nat_table[i].internal_port == src_port &&
            nat_table[i].remote_ip == dst_ip &&
            nat_table[i].remote_port == dst_port &&
            nat_table[i].protocol == proto) {
            return &nat_table[i];
        }
    }
    return NULL;
}

static nat_entry_t *nat_find_inbound(u16 external_port, u32 remote_ip,
                                      u16 remote_port, u8 proto) {
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        if (nat_table[i].active &&
            nat_table[i].external_port == external_port &&
            nat_table[i].remote_ip == remote_ip &&
            nat_table[i].remote_port == remote_port &&
            nat_table[i].protocol == proto) {
            return &nat_table[i];
        }
    }
    return NULL;
}

static nat_entry_t *nat_alloc_entry(void) {
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        if (!nat_table[i].active) return &nat_table[i];
    }
    return NULL;
}

int nat_translate_outbound(pktbuf_t *pkt, u32 wan_ip) {
    if (!nat_enabled || !pkt) return -1;
    if (pkt->len < sizeof(nat_ipv4_hdr_t) + 4) return -1;

    nat_ipv4_hdr_t *ip = (nat_ipv4_hdr_t *)(pkt->data + pkt->head);
    u8 ihl = (ip->version_ihl & 0xF) * 4;
    u8 *transport = pkt->data + pkt->head + ihl;

    u32 src_ip = ip->src;
    u16 src_port = (u16)((transport[0] << 8) | transport[1]);
    u32 dst_ip = ip->dst;
    u16 dst_port = (u16)((transport[2] << 8) | transport[3]);

    /* Look for existing mapping */
    nat_entry_t *entry = nat_find_outbound(src_ip, src_port, dst_ip, dst_port, ip->protocol);
    if (!entry) {
        /* Create new NAT mapping */
        entry = nat_alloc_entry();
        if (!entry) return -1;
        entry->internal_ip = src_ip;
        entry->internal_port = src_port;
        entry->external_ip = wan_ip;
        entry->external_port = alloc_nat_port();
        entry->remote_ip = dst_ip;
        entry->remote_port = dst_port;
        entry->protocol = ip->protocol;
        entry->active = true;
    }
    entry->last_seen = 0;

    /* Rewrite source IP to WAN IP */
    ip->src = wan_ip;
    ip_recalc_checksum(ip);

    /* Rewrite source port in transport header */
    transport[0] = (u8)(entry->external_port >> 8);
    transport[1] = (u8)(entry->external_port & 0xFF);

    /* Zero transport checksum (will be recalculated or left at 0 for UDP) */
    /* For TCP, proper recalc needed — simplified here */

    return 0;
}

int nat_translate_inbound(pktbuf_t *pkt) {
    if (!nat_enabled || !pkt) return -1;
    if (pkt->len < sizeof(nat_ipv4_hdr_t) + 4) return -1;

    nat_ipv4_hdr_t *ip = (nat_ipv4_hdr_t *)(pkt->data + pkt->head);
    u8 ihl = (ip->version_ihl & 0xF) * 4;
    u8 *transport = pkt->data + pkt->head + ihl;

    u16 dst_port = (u16)((transport[2] << 8) | transport[3]);
    u32 src_ip = ip->src;
    u16 src_port = (u16)((transport[0] << 8) | transport[1]);

    /* Look up NAT mapping by external port */
    nat_entry_t *entry = nat_find_inbound(dst_port, src_ip, src_port, ip->protocol);
    if (!entry) return -1; /* No mapping — drop */

    entry->last_seen = 0;

    /* Rewrite destination to internal IP */
    ip->dst = entry->internal_ip;
    ip_recalc_checksum(ip);

    /* Rewrite destination port */
    transport[2] = (u8)(entry->internal_port >> 8);
    transport[3] = (u8)(entry->internal_port & 0xFF);

    return 0;
}

/* Port forwarding (static NAT) */
int nat_add_port_forward(u32 external_ip, u16 external_port,
                         u32 internal_ip, u16 internal_port, u8 protocol) {
    nat_entry_t *entry = nat_alloc_entry();
    if (!entry) return -1;

    entry->external_ip = external_ip;
    entry->external_port = external_port;
    entry->internal_ip = internal_ip;
    entry->internal_port = internal_port;
    entry->remote_ip = 0; /* Any source */
    entry->remote_port = 0;
    entry->protocol = protocol;
    entry->active = true;
    return 0;
}

void nat_cleanup(u32 max_age) {
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        if (nat_table[i].active && nat_table[i].last_seen > max_age) {
            nat_table[i].active = false;
        }
    }
}

u32 nat_active_count(void) {
    u32 count = 0;
    for (int i = 0; i < NAT_MAX_ENTRIES; i++) {
        if (nat_table[i].active) count++;
    }
    return count;
}
