/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/dns.c
 * DNS resolver — handles A record lookups for hostname resolution.
 * Uses UDP port 53 with simple query/response protocol (RFC 1035).
 */
#include "include/packet.h"
#include "../kernel/include/memory.h"

#define DNS_PORT           53
#define DNS_MAX_NAME       256
#define DNS_MAX_CACHE      64
#define DNS_TIMEOUT_MS     5000
#define DNS_MAX_RETRIES    3

/* DNS header */
typedef struct {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} __attribute__((packed)) dns_header_t;

/* DNS query types */
#define DNS_TYPE_A     1
#define DNS_TYPE_AAAA  28
#define DNS_TYPE_CNAME 5

/* DNS response codes */
#define DNS_RCODE_OK     0
#define DNS_RCODE_NXDOM  3

typedef struct {
    char name[DNS_MAX_NAME];
    u32  ip;
    u32  ttl;
    bool valid;
} dns_cache_entry_t;

static dns_cache_entry_t dns_cache[DNS_MAX_CACHE];
static u32 dns_server_ip = 0x01010101; /* 1.1.1.1 default */
static u16 dns_query_id = 1;

extern int udp_send(u32 dst_ip, u16 dst_port, u16 src_port, const u8 *data, u16 len);
extern u32 ipv4_get_addr(void);

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void dns_init(void) {
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        dns_cache[i].valid = false;
    }
}

void dns_set_server(u32 ip) {
    dns_server_ip = ip;
}

/* Encode hostname into DNS wire format (label encoding) */
static int dns_encode_name(const char *name, u8 *buf, int max_len) {
    int pos = 0;
    const char *p = name;

    while (*p && pos < max_len - 2) {
        /* Find next dot or end */
        int label_start = pos++;
        int label_len = 0;
        while (*p && *p != '.' && pos < max_len - 1) {
            buf[pos++] = (u8)*p++;
            label_len++;
        }
        buf[label_start] = (u8)label_len;
        if (*p == '.') p++;
    }
    buf[pos++] = 0; /* Root label */
    return pos;
}

/* Parse DNS response and extract IP */
static int dns_parse_response(const u8 *data, u16 len, u32 *out_ip) {
    if (len < sizeof(dns_header_t)) return -1;

    const dns_header_t *hdr = (const dns_header_t *)data;
    u16 flags = (u16)((hdr->flags >> 8) | (hdr->flags << 8));
    u16 ancount = (u16)((hdr->ancount >> 8) | (hdr->ancount << 8));

    /* Check it's a response */
    if (!(flags & 0x8000)) return -1;
    /* Check no error */
    if ((flags & 0x000F) != DNS_RCODE_OK) return -1;
    if (ancount == 0) return -1;

    /* Skip question section */
    int pos = sizeof(dns_header_t);
    u16 qdcount = (u16)((hdr->qdcount >> 8) | (hdr->qdcount << 8));
    for (int q = 0; q < qdcount && pos < len; q++) {
        /* Skip name */
        while (pos < len && data[pos] != 0) {
            if ((data[pos] & 0xC0) == 0xC0) { pos += 2; goto skip_done; }
            pos += data[pos] + 1;
        }
        pos++; /* Skip null */
skip_done:
        pos += 4; /* Skip QTYPE + QCLASS */
    }

    /* Parse answer records */
    for (int a = 0; a < ancount && pos < len; a++) {
        /* Skip name (might be compressed) */
        if ((data[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < len && data[pos] != 0) pos += data[pos] + 1;
            pos++;
        }

        if (pos + 10 > len) return -1;
        u16 rtype = (u16)((data[pos] << 8) | data[pos + 1]);
        pos += 2;
        pos += 2; /* Skip class */
        pos += 4; /* Skip TTL */
        u16 rdlen = (u16)((data[pos] << 8) | data[pos + 1]);
        pos += 2;

        if (rtype == DNS_TYPE_A && rdlen == 4) {
            /* IPv4 address */
            *out_ip = *(u32 *)&data[pos];
            return 0;
        }
        pos += rdlen;
    }

    return -1;
}

int dns_resolve(const char *hostname, u32 *out_ip) {
    if (!hostname || !out_ip) return -1;

    /* Check cache first */
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        if (dns_cache[i].valid && str_eq(dns_cache[i].name, hostname)) {
            *out_ip = dns_cache[i].ip;
            return 0;
        }
    }

    /* Build DNS query */
    u8 query[512];
    dns_header_t *hdr = (dns_header_t *)query;
    u16 id = dns_query_id++;
    hdr->id = (u16)((id >> 8) | (id << 8));
    hdr->flags = 0x0001; /* RD=1 (recursion desired), network byte order */
    hdr->qdcount = 0x0100; /* 1 question, network byte order */
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    int pos = sizeof(dns_header_t);
    pos += dns_encode_name(hostname, query + pos, (int)sizeof(query) - pos);

    /* QTYPE = A (1) */
    query[pos++] = 0;
    query[pos++] = 1;
    /* QCLASS = IN (1) */
    query[pos++] = 0;
    query[pos++] = 1;

    /* Send query via UDP */
    int ret = udp_send(dns_server_ip, DNS_PORT, 10053, query, (u16)pos);
    if (ret < 0) return -1;

    /* In a real implementation, we would wait for the UDP response
     * and call dns_parse_response. For now, this is the send path
     * and response handling would be triggered by udp_rx delivering
     * to port 10053. */
    (void)dns_parse_response;

    return -1; /* Pending — response will arrive asynchronously */
}

/* Called when DNS response arrives on our source port */
void dns_handle_response(const u8 *data, u16 len, const char *queried_name) {
    u32 ip = 0;
    if (dns_parse_response(data, len, &ip) == 0 && ip != 0) {
        /* Cache the result */
        for (int i = 0; i < DNS_MAX_CACHE; i++) {
            if (!dns_cache[i].valid) {
                int j = 0;
                while (queried_name[j] && j < DNS_MAX_NAME - 1) {
                    dns_cache[i].name[j] = queried_name[j];
                    j++;
                }
                dns_cache[i].name[j] = '\0';
                dns_cache[i].ip = ip;
                dns_cache[i].ttl = 300; /* Default 5 min */
                dns_cache[i].valid = true;
                break;
            }
        }
    }
}

void dns_cache_flush(void) {
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        dns_cache[i].valid = false;
    }
}
