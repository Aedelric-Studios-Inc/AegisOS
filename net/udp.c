/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/udp.c
 * UDP transport layer — connectionless datagram delivery with
 * port-based multiplexing and optional checksum verification.
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define UDP_MAX_SOCKETS 64
#define UDP_RX_BUF_SIZE 4096

typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} __attribute__((packed)) udp_hdr_t;

typedef struct {
    u16 local_port;
    bool bound;
    u8  rx_buf[UDP_RX_BUF_SIZE];
    u32 rx_len;
    u32 rx_src_ip;
    u16 rx_src_port;
    bool rx_ready;
} udp_socket_t;

/* Pseudo-header for checksum */
typedef struct {
    u32 src;
    u32 dst;
    u8  zero;
    u8  protocol;
    u16 length;
} __attribute__((packed)) udp_pseudo_hdr_t;

static udp_socket_t sockets[UDP_MAX_SOCKETS];

extern int ipv4_send(u32 dst, u8 proto, const u8 *payload, u16 len);
extern u32 ipv4_get_addr(void);

void udp_init(void) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        sockets[i].bound = false;
        sockets[i].rx_ready = false;
        sockets[i].rx_len = 0;
    }
}

int udp_bind(u16 port) {
    /* Find free socket slot */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!sockets[i].bound) {
            sockets[i].local_port = port;
            sockets[i].bound = true;
            sockets[i].rx_ready = false;
            sockets[i].rx_len = 0;
            return i; /* Return socket index */
        }
    }
    return -1;
}

void udp_unbind(int sock_id) {
    if (sock_id >= 0 && sock_id < UDP_MAX_SOCKETS) {
        sockets[sock_id].bound = false;
    }
}

void udp_rx(pktbuf_t *pkt, u32 src_ip) {
    if (!pkt || pkt->len < sizeof(udp_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)(pkt->data + pkt->head);
    u16 dst_port = (u16)((hdr->dst_port >> 8) | (hdr->dst_port << 8));
    u16 src_port = (u16)((hdr->src_port >> 8) | (hdr->src_port << 8));
    u16 udp_len = (u16)((hdr->length >> 8) | (hdr->length << 8));
    u16 payload_len = udp_len - sizeof(udp_hdr_t);

    /* Find bound socket for this port */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (sockets[i].bound && sockets[i].local_port == dst_port) {
            /* Deliver to socket */
            u32 copy = payload_len < UDP_RX_BUF_SIZE ? payload_len : UDP_RX_BUF_SIZE;
            const u8 *payload = pkt->data + pkt->head + sizeof(udp_hdr_t);
            for (u32 j = 0; j < copy; j++) {
                sockets[i].rx_buf[j] = payload[j];
            }
            sockets[i].rx_len = copy;
            sockets[i].rx_src_ip = src_ip;
            sockets[i].rx_src_port = src_port;
            sockets[i].rx_ready = true;
            pktbuf_free(pkt);
            return;
        }
    }

    /* No socket — send ICMP port unreachable (handled elsewhere) */
    pktbuf_free(pkt);
}

int udp_send(u32 dst_ip, u16 dst_port, u16 src_port, const u8 *data, u16 len) {
    if (!data || len == 0) return -1;

    u16 total = sizeof(udp_hdr_t) + len;
    u8 packet[1500]; /* Stack buffer for UDP packet */
    if (total > sizeof(packet)) return -1;

    udp_hdr_t *hdr = (udp_hdr_t *)packet;
    hdr->src_port = (u16)((src_port >> 8) | (src_port << 8));
    hdr->dst_port = (u16)((dst_port >> 8) | (dst_port << 8));
    hdr->length = (u16)((total >> 8) | (total << 8));
    hdr->checksum = 0; /* Optional for IPv4 */

    /* Copy payload */
    u8 *payload = packet + sizeof(udp_hdr_t);
    for (u16 i = 0; i < len; i++) payload[i] = data[i];

    /* Compute checksum */
    udp_pseudo_hdr_t pseudo;
    pseudo.src = ipv4_get_addr();
    pseudo.dst = dst_ip;
    pseudo.zero = 0;
    pseudo.protocol = 17;
    pseudo.length = hdr->length;

    u32 sum = 0;
    const u16 *p = (const u16 *)&pseudo;
    for (int i = 0; i < 6; i++) sum += p[i];
    p = (const u16 *)packet;
    u16 remaining = total;
    while (remaining > 1) { sum += *p++; remaining -= 2; }
    if (remaining) sum += *(const u8 *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    hdr->checksum = (u16)~sum;
    if (hdr->checksum == 0) hdr->checksum = 0xFFFF;

    return ipv4_send(dst_ip, 17, packet, total);
}

int udp_recv(int sock_id, u8 *buf, u16 max_len, u32 *src_ip, u16 *src_port) {
    if (sock_id < 0 || sock_id >= UDP_MAX_SOCKETS) return -1;
    if (!sockets[sock_id].bound || !sockets[sock_id].rx_ready) return -1;

    u32 copy = sockets[sock_id].rx_len < max_len ? sockets[sock_id].rx_len : max_len;
    for (u32 i = 0; i < copy; i++) buf[i] = sockets[sock_id].rx_buf[i];
    if (src_ip) *src_ip = sockets[sock_id].rx_src_ip;
    if (src_port) *src_port = sockets[sock_id].rx_src_port;
    sockets[sock_id].rx_ready = false;
    sockets[sock_id].rx_len = 0;
    return (int)copy;
}
