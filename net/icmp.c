/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/icmp.c
 * ICMP protocol handling — echo request/reply (ping), destination
 * unreachable, time exceeded, and other control messages.
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

typedef struct {
    u8  type;
    u8  code;
    u16 checksum;
    u32 rest;  /* Varies by type: ID+Seq for echo, unused for others */
} __attribute__((packed)) icmp_hdr_t;

#define ICMP_ECHO_REQUEST    8
#define ICMP_ECHO_REPLY      0
#define ICMP_DEST_UNREACH    3
#define ICMP_TIME_EXCEEDED  11
#define ICMP_REDIRECT        5

/* ICMP unreachable codes */
#define ICMP_NET_UNREACH     0
#define ICMP_HOST_UNREACH    1
#define ICMP_PROTO_UNREACH   2
#define ICMP_PORT_UNREACH    3

extern int ipv4_send(u32 dst, u8 proto, const u8 *payload, u16 len);
extern u32 ipv4_get_addr(void);

static u16 icmp_checksum(const void *data, u16 len) {
    const u16 *ptr = (const u16 *)data;
    u32 sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) sum += *(const u8 *)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

void icmp_rx(pktbuf_t *pkt, u32 src_ip) {
    if (!pkt || pkt->len < sizeof(icmp_hdr_t)) {
        pktbuf_free(pkt);
        return;
    }

    icmp_hdr_t *hdr = (icmp_hdr_t *)(pkt->data + pkt->head);

    /* Verify checksum */
    u16 saved = hdr->checksum;
    hdr->checksum = 0;
    u16 calc = icmp_checksum(pkt->data + pkt->head, pkt->len);
    hdr->checksum = saved;
    if (calc != saved) {
        pktbuf_free(pkt);
        return;
    }

    switch (hdr->type) {
    case ICMP_ECHO_REQUEST: {
        /* Send echo reply — swap src/dst, change type to 0 */
        u8 reply[1500];
        u16 reply_len = pkt->len;
        if (reply_len > sizeof(reply)) reply_len = sizeof(reply);

        /* Copy entire ICMP payload */
        const u8 *src = pkt->data + pkt->head;
        for (u16 i = 0; i < reply_len; i++) reply[i] = src[i];

        icmp_hdr_t *reply_hdr = (icmp_hdr_t *)reply;
        reply_hdr->type = ICMP_ECHO_REPLY;
        reply_hdr->code = 0;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = icmp_checksum(reply, reply_len);

        ipv4_send(src_ip, 1, reply, reply_len);
        break;
    }
    case ICMP_ECHO_REPLY:
        /* Could notify a waiting ping() call */
        break;
    case ICMP_DEST_UNREACH:
        /* Log or notify upper layer */
        break;
    case ICMP_TIME_EXCEEDED:
        /* Traceroute response */
        break;
    default:
        break;
    }

    pktbuf_free(pkt);
}

int icmp_send_echo_request(u32 dst_ip, u16 id, u16 seq) {
    u8 packet[64];
    icmp_hdr_t *hdr = (icmp_hdr_t *)packet;
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    /* Pack ID and sequence into the "rest" field */
    hdr->rest = ((u32)id << 16) | (u32)seq;

    /* Fill payload with pattern */
    for (int i = sizeof(icmp_hdr_t); i < 64; i++) {
        packet[i] = (u8)(i & 0xFF);
    }

    hdr->checksum = icmp_checksum(packet, 64);
    return ipv4_send(dst_ip, 1, packet, 64);
}

int icmp_send_dest_unreachable(u32 dst_ip, u8 code, const u8 *orig_header, u16 orig_len) {
    u8 packet[576]; /* Minimum MTU */
    icmp_hdr_t *hdr = (icmp_hdr_t *)packet;
    hdr->type = ICMP_DEST_UNREACH;
    hdr->code = code;
    hdr->checksum = 0;
    hdr->rest = 0;

    /* Include original IP header + 8 bytes of original payload */
    u16 copy = orig_len < 28 ? orig_len : 28;
    for (u16 i = 0; i < copy; i++) {
        packet[sizeof(icmp_hdr_t) + i] = orig_header[i];
    }

    u16 total = sizeof(icmp_hdr_t) + copy;
    hdr->checksum = icmp_checksum(packet, total);
    return ipv4_send(dst_ip, 1, packet, total);
}
