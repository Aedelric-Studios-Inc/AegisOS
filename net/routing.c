/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/routing.c */
#include "include/routing.h"
#include "../../AegisOS/kernel/include/memory.h"

static route_t *route_table;

int routing_init(void) { route_table = NULL; return 0; }

route_t *routing_lookup(u32 dst_ip) {
    route_t *best = NULL;
    for (route_t *r = route_table; r; r = r->next) {
        if ((dst_ip & r->netmask) == (r->network & r->netmask)) {
            if (!best || r->netmask > best->netmask) best = r;
        }
    }
    return best;
}

int routing_add(route_t *route) {
    route->next = route_table;
    route_table = route;
    return 0;
}

int routing_del(u32 network, u32 netmask) {
    (void)network; (void)netmask;
    return 0;
}
