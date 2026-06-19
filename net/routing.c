/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/routing.c
 * IP routing table — longest-prefix match routing with metric-based
 * tie breaking. Supports static and dynamic route management.
 */
#include "include/routing.h"
#include "../kernel/include/memory.h"

#define ROUTE_MAX_ENTRIES 128

static route_t route_storage[ROUTE_MAX_ENTRIES];
static route_t *route_table;
static u32 route_count = 0;

int routing_init(void) {
    route_table = NULL;
    route_count = 0;
    return 0;
}

route_t *routing_lookup(u32 dst_ip) {
    route_t *best = NULL;
    u32 best_mask = 0;
    u32 best_metric = 0xFFFFFFFF;

    for (route_t *r = route_table; r; r = r->next) {
        if ((dst_ip & r->netmask) == (r->network & r->netmask)) {
            /* Longest prefix match */
            if (r->netmask > best_mask ||
                (r->netmask == best_mask && r->metric < best_metric)) {
                best = r;
                best_mask = r->netmask;
                best_metric = r->metric;
            }
        }
    }
    return best;
}

int routing_add(route_t *route) {
    if (!route || route_count >= ROUTE_MAX_ENTRIES) return -1;

    /* Check for duplicate */
    for (route_t *r = route_table; r; r = r->next) {
        if (r->network == route->network && r->netmask == route->netmask &&
            r->gateway == route->gateway) {
            /* Update metric */
            r->metric = route->metric;
            return 0;
        }
    }

    /* Copy to storage */
    route_storage[route_count] = *route;
    route_t *stored = &route_storage[route_count];
    stored->next = route_table;
    route_table = stored;
    route_count++;
    return 0;
}

int routing_del(u32 network, u32 netmask) {
    route_t *prev = NULL;
    for (route_t *r = route_table; r; r = r->next) {
        if (r->network == network && r->netmask == netmask) {
            if (prev) {
                prev->next = r->next;
            } else {
                route_table = r->next;
            }
            return 0;
        }
        prev = r;
    }
    return -1;
}

/* Set default gateway */
int routing_set_default(u32 gateway, u32 iface_ip) {
    route_t route = {
        .network = 0,
        .netmask = 0,
        .gateway = gateway,
        .iface_ip = iface_ip,
        .metric = 100,
        .next = NULL,
    };
    return routing_add(&route);
}

u32 routing_count(void) {
    return route_count;
}
