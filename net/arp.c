/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/arp.c
 * Address Resolution Protocol — maps IPv4 addresses to MAC addresses.
 * Maintains a cache and handles request/reply exchanges.
 */
#include "include/packet.h"
#include "../hal/include/ethernet.h"

#define ARP_MAX_CACHE 64
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2
#define ARP_HW_ETHER   1
#define ARP_PROTO_IPV4  0x0800

typedef struct {
    u16 hw_type;
    u16 proto_type;
    u8  hw_len;
    u8  proto_len;
    u16 operation;
    u8  sender_mac[6];
    u32 sender_ip;
    u8  target_mac[6];
    u32 target_ip;
} __attribute__((packed)) arp_packet_t;

typedef struct {
    u32 ip;
    u8  mac[6];
    bool valid;
    u32 age;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_MAX_CACHE];
static u32 arp_age_counter = 0;

/* External interfaces */
extern u32 ipv4_get_addr(void);
extern void ethernet_get_local_mac(u8 *mac);
extern int ethernet_tx(const u8 *dst_mac, u16 ethertype, pktbuf_t *pkt);

void arp_init(void) {
    for (int i = 0; i < ARP_MAX_CACHE; i++) {
        arp_cache[i].valid = false;
    }
}

void arp_cache_set(u32 ip, const u8 *mac) {
    /* Update existing entry */
    for (int i = 0; i < ARP_MAX_CACHE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            for (int j = 0; j < 6; j++) arp_cache[i].mac[j] = mac[j];
            arp_cache[i].age = ++arp_age_counter;
            return;
        }
    }
    /* Find empty slot */
    for (int i = 0; i < ARP_MAX_CACHE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            for (int j = 0; j < 6; j++) arp_cache[i].mac[j] = mac[j];
            arp_cache[i].valid = true;
            arp_cache[i].age = ++arp_age_counter;
            return;
        }
    }
    /* Cache full — evict oldest */
    u32 oldest_age = 0xFFFFFFFF;
    int oldest_idx = 0;
    for (int i = 0; i < ARP_MAX_CACHE; i++) {
        if (arp_cache[i].age < oldest_age) {
            oldest_age = arp_cache[i].age;
            oldest_idx = i;
        }
    }
    arp_cache[oldest_idx].ip = ip;
    for (int j = 0; j < 6; j++) arp_cache[oldest_idx].mac[j] = mac[j];
    arp_cache[oldest_idx].age = ++arp_age_counter;
}

const u8 *arp_cache_lookup(u32 ip) {
    for (int i = 0; i < ARP_MAX_CACHE; i++)
        if (arp_cache[i].valid && arp_cache[i].ip == ip)
            return arp_cache[i].mac;
    return NULL;
}

static void arp_send_reply(u32 target_ip, const u8 *target_mac) {
    pktbuf_t *pkt = pktbuf_alloc();
    if (!pkt) return;

    arp_packet_t *arp = (arp_packet_t *)(pkt->data + pkt->head);
    pkt->len = sizeof(arp_packet_t);

    u8 our_mac[6];
    ethernet_get_local_mac(our_mac);
    u32 our_ip = ipv4_get_addr();

    arp->hw_type = (ARP_HW_ETHER >> 8) | (ARP_HW_ETHER << 8);
    arp->proto_type = (ARP_PROTO_IPV4 >> 8) | (ARP_PROTO_IPV4 << 8);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->operation = (ARP_OP_REPLY >> 8) | (ARP_OP_REPLY << 8);

    for (int i = 0; i < 6; i++) {
        arp->sender_mac[i] = our_mac[i];
        arp->target_mac[i] = target_mac[i];
    }
    arp->sender_ip = our_ip;
    arp->target_ip = target_ip;

    ethernet_tx(target_mac, 0x0806, pkt);
}

int arp_send_request(u32 target_ip) {
    pktbuf_t *pkt = pktbuf_alloc();
    if (!pkt) return -1;

    arp_packet_t *arp = (arp_packet_t *)(pkt->data + pkt->head);
    pkt->len = sizeof(arp_packet_t);

    u8 our_mac[6];
    ethernet_get_local_mac(our_mac);
    u32 our_ip = ipv4_get_addr();

    arp->hw_type = (ARP_HW_ETHER >> 8) | (ARP_HW_ETHER << 8);
    arp->proto_type = (ARP_PROTO_IPV4 >> 8) | (ARP_PROTO_IPV4 << 8);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->operation = (ARP_OP_REQUEST >> 8) | (ARP_OP_REQUEST << 8);

    for (int i = 0; i < 6; i++) {
        arp->sender_mac[i] = our_mac[i];
        arp->target_mac[i] = 0xFF; /* Broadcast */
    }
    arp->sender_ip = our_ip;
    arp->target_ip = target_ip;

    u8 broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return ethernet_tx(broadcast, 0x0806, pkt);
}

void arp_rx(pktbuf_t *pkt) {
    if (!pkt || pkt->len < sizeof(arp_packet_t)) {
        pktbuf_free(pkt);
        return;
    }

    arp_packet_t *arp = (arp_packet_t *)(pkt->data + pkt->head);
    u16 op = (u16)((arp->operation >> 8) | (arp->operation << 8));

    /* Always learn sender's MAC */
    arp_cache_set(arp->sender_ip, arp->sender_mac);

    u32 our_ip = ipv4_get_addr();

    if (op == ARP_OP_REQUEST && arp->target_ip == our_ip) {
        /* Someone is asking for our MAC — send reply */
        arp_send_reply(arp->sender_ip, arp->sender_mac);
    }

    pktbuf_free(pkt);
}
