/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/tcp.c
 * TCP transport layer — connection management, reliable delivery,
 * flow control, and congestion avoidance.
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define TCP_FLAG_FIN  0x01
#define TCP_FLAG_SYN  0x02
#define TCP_FLAG_RST  0x04
#define TCP_FLAG_PSH  0x08
#define TCP_FLAG_ACK  0x10
#define TCP_FLAG_URG  0x20

#define TCP_MAX_CONNECTIONS 128
#define TCP_WINDOW_SIZE     65535
#define TCP_MSS             1460
#define TCP_RETRANSMIT_MS   1000
#define TCP_MAX_RETRIES     5

typedef struct {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_offset;
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} __attribute__((packed)) tcp_hdr_t;

typedef enum {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
} tcp_state_t;

typedef struct tcp_conn {
    u32 local_ip;
    u16 local_port;
    u32 remote_ip;
    u16 remote_port;
    tcp_state_t state;
    u32 snd_nxt;      /* Next sequence to send */
    u32 snd_una;      /* Oldest unacknowledged */
    u32 rcv_nxt;      /* Next expected receive seq */
    u16 snd_wnd;      /* Send window */
    u16 rcv_wnd;      /* Receive window */
    u8  rx_buf[8192]; /* Receive buffer */
    u32 rx_len;
    u8  tx_buf[8192]; /* Transmit buffer */
    u32 tx_len;
    u32 retransmit_count;
    bool active;
} tcp_conn_t;

/* Pseudo-header for checksum */
typedef struct {
    u32 src;
    u32 dst;
    u8  zero;
    u8  protocol;
    u16 length;
} __attribute__((packed)) tcp_pseudo_hdr_t;

static tcp_conn_t connections[TCP_MAX_CONNECTIONS];
static u16 next_ephemeral_port = 49152;

extern int ipv4_send(u32 dst, u8 proto, const u8 *payload, u16 len);
extern u32 ipv4_get_addr(void);

static u16 tcp_checksum(u32 src_ip, u32 dst_ip, const u8 *tcp_data, u16 tcp_len) {
    tcp_pseudo_hdr_t pseudo;
    pseudo.src = src_ip;
    pseudo.dst = dst_ip;
    pseudo.zero = 0;
    pseudo.protocol = 6;
    pseudo.length = (u16)((tcp_len >> 8) | (tcp_len << 8));

    u32 sum = 0;
    const u16 *p = (const u16 *)&pseudo;
    for (int i = 0; i < 6; i++) sum += p[i];

    p = (const u16 *)tcp_data;
    u16 remaining = tcp_len;
    while (remaining > 1) { sum += *p++; remaining -= 2; }
    if (remaining) sum += *(const u8 *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

void tcp_init(void) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        connections[i].active = false;
        connections[i].state = TCP_CLOSED;
    }
}

static tcp_conn_t *tcp_find_conn(u32 remote_ip, u16 remote_port, u16 local_port) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (connections[i].active &&
            connections[i].remote_ip == remote_ip &&
            connections[i].remote_port == remote_port &&
            connections[i].local_port == local_port) {
            return &connections[i];
        }
    }
    /* Check for listening sockets */
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (connections[i].active &&
            connections[i].state == TCP_LISTEN &&
            connections[i].local_port == local_port) {
            return &connections[i];
        }
    }
    return NULL;
}

static tcp_conn_t *tcp_alloc_conn(void) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (!connections[i].active) {
            connections[i].active = true;
            connections[i].rx_len = 0;
            connections[i].tx_len = 0;
            connections[i].retransmit_count = 0;
            return &connections[i];
        }
    }
    return NULL;
}

static int tcp_send_segment(tcp_conn_t *conn, u8 flags, const u8 *data, u16 len) {
    u8 segment[TCP_MSS + 20]; /* Max TCP header (20) + data */
    tcp_hdr_t *hdr = (tcp_hdr_t *)segment;

    hdr->src_port = (u16)((conn->local_port >> 8) | (conn->local_port << 8));
    hdr->dst_port = (u16)((conn->remote_port >> 8) | (conn->remote_port << 8));
    hdr->seq = __builtin_bswap32(conn->snd_nxt);
    hdr->ack = __builtin_bswap32(conn->rcv_nxt);
    hdr->data_offset = 0x50; /* 5 * 4 = 20 bytes, no options */
    hdr->flags = flags;
    hdr->window = (u16)((conn->rcv_wnd >> 8) | (conn->rcv_wnd << 8));
    hdr->checksum = 0;
    hdr->urgent = 0;

    /* Copy payload */
    if (data && len > 0) {
        for (u16 i = 0; i < len; i++) segment[20 + i] = data[i];
    }

    u16 total = 20 + len;
    hdr->checksum = tcp_checksum(conn->local_ip, conn->remote_ip, segment, total);

    /* Update sequence number */
    if (flags & TCP_FLAG_SYN) conn->snd_nxt++;
    if (flags & TCP_FLAG_FIN) conn->snd_nxt++;
    conn->snd_nxt += len;

    return ipv4_send(conn->remote_ip, 6, segment, total);
}

void tcp_rx(pktbuf_t *pkt, u32 src_ip) {
    if (!pkt || pkt->len < sizeof(tcp_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    tcp_hdr_t *hdr = (tcp_hdr_t *)(pkt->data + pkt->head);
    u16 src_port = (u16)((hdr->src_port >> 8) | (hdr->src_port << 8));
    u16 dst_port = (u16)((hdr->dst_port >> 8) | (hdr->dst_port << 8));
    u32 seq = __builtin_bswap32(hdr->seq);
    u32 ack = __builtin_bswap32(hdr->ack);
    u8 flags = hdr->flags;
    u8 data_off = (hdr->data_offset >> 4) * 4;

    tcp_conn_t *conn = tcp_find_conn(src_ip, src_port, dst_port);

    if (!conn) {
        /* No connection — send RST if not RST already */
        if (!(flags & TCP_FLAG_RST)) {
            /* Send RST */
        }
        pktbuf_free(pkt);
        return;
    }

    switch (conn->state) {
    case TCP_LISTEN:
        if (flags & TCP_FLAG_SYN) {
            /* Incoming connection — create new connection */
            tcp_conn_t *new_conn = tcp_alloc_conn();
            if (!new_conn) { pktbuf_free(pkt); return; }
            new_conn->local_ip = ipv4_get_addr();
            new_conn->local_port = dst_port;
            new_conn->remote_ip = src_ip;
            new_conn->remote_port = src_port;
            new_conn->state = TCP_SYN_RECEIVED;
            new_conn->rcv_nxt = seq + 1;
            new_conn->snd_nxt = 1000; /* ISN — should be random */
            new_conn->snd_wnd = TCP_WINDOW_SIZE;
            new_conn->rcv_wnd = TCP_WINDOW_SIZE;
            tcp_send_segment(new_conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        }
        break;

    case TCP_SYN_SENT:
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            conn->rcv_nxt = seq + 1;
            conn->snd_una = ack;
            conn->state = TCP_ESTABLISHED;
            tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
        }
        break;

    case TCP_SYN_RECEIVED:
        if (flags & TCP_FLAG_ACK) {
            conn->snd_una = ack;
            conn->state = TCP_ESTABLISHED;
        }
        break;

    case TCP_ESTABLISHED:
        if (flags & TCP_FLAG_FIN) {
            conn->rcv_nxt = seq + 1;
            conn->state = TCP_CLOSE_WAIT;
            tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
        } else if (flags & TCP_FLAG_ACK) {
            conn->snd_una = ack;
            /* Process incoming data */
            u16 payload_len = pkt->len - data_off;
            if (payload_len > 0 && seq == conn->rcv_nxt) {
                const u8 *payload = pkt->data + pkt->head + data_off;
                u32 space = sizeof(conn->rx_buf) - conn->rx_len;
                u32 copy = payload_len < space ? payload_len : space;
                for (u32 i = 0; i < copy; i++) {
                    conn->rx_buf[conn->rx_len + i] = payload[i];
                }
                conn->rx_len += copy;
                conn->rcv_nxt += copy;
                /* Send ACK */
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
            }
        }
        break;

    case TCP_FIN_WAIT_1:
        if ((flags & TCP_FLAG_ACK) && (flags & TCP_FLAG_FIN)) {
            conn->rcv_nxt = seq + 1;
            conn->state = TCP_TIME_WAIT;
            tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
        } else if (flags & TCP_FLAG_ACK) {
            conn->state = TCP_FIN_WAIT_2;
        } else if (flags & TCP_FLAG_FIN) {
            conn->rcv_nxt = seq + 1;
            conn->state = TCP_CLOSING;
            tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
        }
        break;

    case TCP_FIN_WAIT_2:
        if (flags & TCP_FLAG_FIN) {
            conn->rcv_nxt = seq + 1;
            conn->state = TCP_TIME_WAIT;
            tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
        }
        break;

    case TCP_CLOSE_WAIT:
        /* Waiting for application to close */
        break;

    case TCP_LAST_ACK:
        if (flags & TCP_FLAG_ACK) {
            conn->state = TCP_CLOSED;
            conn->active = false;
        }
        break;

    default:
        break;
    }

    pktbuf_free(pkt);
}

/* Public API for user-space socket layer */
int tcp_listen(u16 port) {
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) return -1;
    conn->local_ip = ipv4_get_addr();
    conn->local_port = port;
    conn->state = TCP_LISTEN;
    conn->rcv_wnd = TCP_WINDOW_SIZE;
    return 0;
}

int tcp_connect(u32 remote_ip, u16 remote_port) {
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) return -1;
    conn->local_ip = ipv4_get_addr();
    conn->local_port = next_ephemeral_port++;
    conn->remote_ip = remote_ip;
    conn->remote_port = remote_port;
    conn->state = TCP_SYN_SENT;
    conn->snd_nxt = 5000; /* ISN */
    conn->rcv_wnd = TCP_WINDOW_SIZE;
    conn->snd_wnd = TCP_WINDOW_SIZE;
    tcp_send_segment(conn, TCP_FLAG_SYN, NULL, 0);
    return 0;
}

int tcp_send(u16 local_port, u32 remote_ip, u16 remote_port,
             const u8 *data, u16 len) {
    tcp_conn_t *conn = tcp_find_conn(remote_ip, remote_port, local_port);
    if (!conn || conn->state != TCP_ESTABLISHED) return -1;
    /* Segment data into MSS-sized chunks */
    u16 offset = 0;
    while (offset < len) {
        u16 chunk = (len - offset > TCP_MSS) ? TCP_MSS : (len - offset);
        tcp_send_segment(conn, TCP_FLAG_ACK | TCP_FLAG_PSH, data + offset, chunk);
        offset += chunk;
    }
    return 0;
}

int tcp_close(u16 local_port, u32 remote_ip, u16 remote_port) {
    tcp_conn_t *conn = tcp_find_conn(remote_ip, remote_port, local_port);
    if (!conn) return -1;
    if (conn->state == TCP_ESTABLISHED) {
        conn->state = TCP_FIN_WAIT_1;
        tcp_send_segment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
    } else if (conn->state == TCP_CLOSE_WAIT) {
        conn->state = TCP_LAST_ACK;
        tcp_send_segment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
    }
    return 0;
}
