/* SPDX-License-Identifier: Proprietary */
#pragma once
/* net/include/routing.h — IP routing table interface */

#include "../../kernel/include/types.h"

typedef struct route {
    u32  network;
    u32  netmask;
    u32  gateway;
    u32  iface_ip;
    u32  metric;
    struct route *next;
} route_t;

int      routing_init(void);
route_t *routing_lookup(u32 dst_ip);
int      routing_add(route_t *route);
int      routing_del(u32 network, u32 netmask);
