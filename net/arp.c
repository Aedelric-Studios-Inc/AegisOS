/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/arp.c */
#include "include/packet.h"

#define ARP_MAX_CACHE 64

typedef struct { u32 ip; u8 mac[6]; bool valid; } arp_entry_t;
static arp_entry_t arp_cache[ARP_MAX_CACHE];

void arp_cache_set(u32 ip, const u8 *mac) {
    for (int i = 0; i < ARP_MAX_CACHE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
            arp_cache[i].ip = ip;
            for (int j = 0; j < 6; j++) arp_cache[i].mac[j] = mac[j];
            arp_cache[i].valid = true;
            return;
        }
    }
}

const u8 *arp_cache_lookup(u32 ip) {
    for (int i = 0; i < ARP_MAX_CACHE; i++)
        if (arp_cache[i].valid && arp_cache[i].ip == ip)
            return arp_cache[i].mac;
    return NULL;
}
