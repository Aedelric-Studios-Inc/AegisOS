/* SPDX-License-Identifier: Proprietary */
#pragma once
/* ethernet.h — Ethernet driver interface */

#include "../../kernel/include/types.h"

#define ETH_ALEN 6
#define ETH_MTU  1500

typedef struct {
    u8 mac[ETH_ALEN];
} eth_addr_t;

int  ethernet_init(void);
int  ethernet_send(const u8 *frame, u16 len);
int  ethernet_recv(u8 *buf, u16 *len);
void ethernet_get_mac(eth_addr_t *out);
