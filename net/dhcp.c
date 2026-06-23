/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/dhcp.c
 * DHCP client — automatic IPv4 address configuration using the
 * DHCP protocol (RFC 2131). Implements DISCOVER/OFFER/REQUEST/ACK
 * state machine for lease acquisition and renewal.
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_DECLINE   4
#define DHCP_ACK       5
#define DHCP_NAK       6
#define DHCP_RELEASE   7

#define DHCP_MAGIC_COOKIE  0x63825363

typedef struct {
    u8  op;
    u8  htype;
    u8  hlen;
    u8  hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u32 ciaddr;
    u32 yiaddr;
    u32 siaddr;
    u32 giaddr;
    u8  chaddr[16];
    u8  sname[64];
    u8  file[128];
    u32 magic_cookie;
    u8  options[312];
} __attribute__((packed)) dhcp_packet_t;

typedef enum {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
} dhcp_state_t;

typedef struct {
    u32 offered_ip;
    u32 server_ip;
    u32 subnet_mask;
    u32 gateway;
    u32 dns_server;
    u32 lease_time;
    u32 xid;
    dhcp_state_t state;
} dhcp_context_t;

static dhcp_context_t dhcp_ctx;

extern int udp_send(u32 dst_ip, u16 dst_port, u16 src_port, const u8 *data, u16 len);
extern void ipv4_set_addr(u32 ip);
extern void ipv4_set_netmask(u32 mask);
extern void ipv4_set_gateway(u32 gw);
extern void ethernet_get_local_mac(u8 *mac);

static void dhcp_add_option(u8 *opts, int *off, u8 type, u8 len, const u8 *data) {
    opts[(*off)++] = type;
    opts[(*off)++] = len;
    for (int i = 0; i < len; i++) opts[(*off)++] = data[i];
}

static int dhcp_build_packet(dhcp_packet_t *pkt, u8 msg_type) {
    /* Zero out packet */
    u8 *p = (u8 *)pkt;
    for (int i = 0; i < (int)sizeof(dhcp_packet_t); i++) p[i] = 0;

    pkt->op = 1;     /* BOOTREQUEST */
    pkt->htype = 1;  /* Ethernet */
    pkt->hlen = 6;
    pkt->hops = 0;
    pkt->xid = dhcp_ctx.xid;
    pkt->flags = 0x0080; /* Broadcast flag (network byte order) */

    /* Our MAC address */
    u8 mac[6];
    ethernet_get_local_mac(mac);
    for (int i = 0; i < 6; i++) pkt->chaddr[i] = mac[i];

    pkt->magic_cookie = DHCP_MAGIC_COOKIE;

    /* Options */
    int off = 0;
    /* Message type */
    dhcp_add_option(pkt->options, &off, 53, 1, &msg_type);

    if (msg_type == DHCP_REQUEST) {
        /* Requested IP */
        u8 ip_bytes[4];
        ip_bytes[0] = (u8)(dhcp_ctx.offered_ip & 0xFF);
        ip_bytes[1] = (u8)((dhcp_ctx.offered_ip >> 8) & 0xFF);
        ip_bytes[2] = (u8)((dhcp_ctx.offered_ip >> 16) & 0xFF);
        ip_bytes[3] = (u8)((dhcp_ctx.offered_ip >> 24) & 0xFF);
        dhcp_add_option(pkt->options, &off, 50, 4, ip_bytes);

        /* Server identifier */
        ip_bytes[0] = (u8)(dhcp_ctx.server_ip & 0xFF);
        ip_bytes[1] = (u8)((dhcp_ctx.server_ip >> 8) & 0xFF);
        ip_bytes[2] = (u8)((dhcp_ctx.server_ip >> 16) & 0xFF);
        ip_bytes[3] = (u8)((dhcp_ctx.server_ip >> 24) & 0xFF);
        dhcp_add_option(pkt->options, &off, 54, 4, ip_bytes);
    }

    /* Parameter request list */
    u8 params[] = {1, 3, 6, 51}; /* Subnet, Router, DNS, Lease Time */
    dhcp_add_option(pkt->options, &off, 55, sizeof(params), params);

    /* End option */
    pkt->options[off++] = 255;

    return (int)sizeof(dhcp_packet_t) - (int)(312 - off);
}

int dhcp_discover(void) {
    /* Generate transaction ID */
    static u32 xid_seed = 0x12345678;
    xid_seed = xid_seed * 1103515245 + 12345;
    dhcp_ctx.xid = xid_seed;
    dhcp_ctx.state = DHCP_STATE_SELECTING;

    dhcp_packet_t pkt;
    int len = dhcp_build_packet(&pkt, DHCP_DISCOVER);

    /* Send as UDP broadcast */
    return udp_send(0xFFFFFFFF, DHCP_SERVER_PORT, DHCP_CLIENT_PORT,
                    (const u8 *)&pkt, (u16)len);
}

int dhcp_request(u32 offered_ip) {
    dhcp_ctx.offered_ip = offered_ip;
    dhcp_ctx.state = DHCP_STATE_REQUESTING;

    dhcp_packet_t pkt;
    int len = dhcp_build_packet(&pkt, DHCP_REQUEST);

    return udp_send(0xFFFFFFFF, DHCP_SERVER_PORT, DHCP_CLIENT_PORT,
                    (const u8 *)&pkt, (u16)len);
}

void dhcp_handle_response(const u8 *data, u16 len) {
    if (len < sizeof(dhcp_packet_t) - 312) return;

    const dhcp_packet_t *pkt = (const dhcp_packet_t *)data;
    if (pkt->op != 2) return; /* Must be BOOTREPLY */
    if (pkt->xid != dhcp_ctx.xid) return;

    /* Parse options */
    u8 msg_type = 0;
    const u8 *opts = pkt->options;
    int i = 0;
    while (i < 312 && opts[i] != 255) {
        u8 opt_type = opts[i++];
        if (opt_type == 0) continue; /* Padding */
        u8 opt_len = opts[i++];

        switch (opt_type) {
        case 53: /* Message type */
            msg_type = opts[i];
            break;
        case 1:  /* Subnet mask */
            if (opt_len >= 4) {
                dhcp_ctx.subnet_mask = *(u32 *)&opts[i];
            }
            break;
        case 3:  /* Router */
            if (opt_len >= 4) {
                dhcp_ctx.gateway = *(u32 *)&opts[i];
            }
            break;
        case 6:  /* DNS */
            if (opt_len >= 4) {
                dhcp_ctx.dns_server = *(u32 *)&opts[i];
            }
            break;
        case 51: /* Lease time */
            if (opt_len >= 4) {
                dhcp_ctx.lease_time = __builtin_bswap32(*(u32 *)&opts[i]);
            }
            break;
        case 54: /* Server identifier */
            if (opt_len >= 4) {
                dhcp_ctx.server_ip = *(u32 *)&opts[i];
            }
            break;
        }
        i += opt_len;
    }

    switch (dhcp_ctx.state) {
    case DHCP_STATE_SELECTING:
        if (msg_type == DHCP_OFFER) {
            dhcp_ctx.offered_ip = pkt->yiaddr;
            dhcp_request(pkt->yiaddr);
        }
        break;
    case DHCP_STATE_REQUESTING:
        if (msg_type == DHCP_ACK) {
            /* Configure interface */
            ipv4_set_addr(dhcp_ctx.offered_ip);
            ipv4_set_netmask(dhcp_ctx.subnet_mask);
            ipv4_set_gateway(dhcp_ctx.gateway);
            dhcp_ctx.state = DHCP_STATE_BOUND;
        } else if (msg_type == DHCP_NAK) {
            dhcp_ctx.state = DHCP_STATE_INIT;
            dhcp_discover(); /* Retry */
        }
        break;
    default:
        break;
    }
}

int dhcp_release(void) {
    if (dhcp_ctx.state != DHCP_STATE_BOUND) return -1;

    dhcp_packet_t pkt;
    int len = dhcp_build_packet(&pkt, DHCP_RELEASE);
    pkt.ciaddr = dhcp_ctx.offered_ip;

    dhcp_ctx.state = DHCP_STATE_INIT;
    ipv4_set_addr(0);

    return udp_send(dhcp_ctx.server_ip, DHCP_SERVER_PORT, DHCP_CLIENT_PORT,
                    (const u8 *)&pkt, (u16)len);
}

dhcp_state_t dhcp_get_state(void) {
    return dhcp_ctx.state;
}

u32 dhcp_get_assigned_ip(void) {
    return dhcp_ctx.offered_ip;
}
