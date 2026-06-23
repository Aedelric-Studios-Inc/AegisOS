/* SPDX-License-Identifier: Proprietary */
/* AegisOS — tests/net/test_net.c
 * Unit tests for the networking subsystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- One's-complement checksum (IP/TCP/UDP header) ---- */

static uint16_t inet_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += ((uint16_t)data[i] << 8) | data[i+1];
    if (len & 1) sum += (uint16_t)data[len-1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ---- ARP cache simulation ---- */

#define ARP_CACHE_SIZE 8

typedef struct { uint32_t ip; uint8_t mac[6]; int valid; } arp_entry_t;
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static void arp_insert(uint32_t ip, const uint8_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
            arp_cache[i].ip    = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
}

static const uint8_t *arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip)
            return arp_cache[i].mac;
    }
    return NULL;
}

/* ---- Firewall rule simulation ---- */

typedef struct { uint32_t src; uint32_t dst; uint16_t dport; int allow; } fw_rule_t;
#define FW_MAX_RULES 16
static fw_rule_t fw_rules[FW_MAX_RULES];
static int fw_rule_count = 0;

static void fw_add_rule(uint32_t src, uint32_t dst, uint16_t dport, int allow) {
    if (fw_rule_count < FW_MAX_RULES) {
        fw_rules[fw_rule_count++] = (fw_rule_t){ src, dst, dport, allow };
    }
}

static int fw_check(uint32_t src, uint32_t dst, uint16_t dport) {
    for (int i = 0; i < fw_rule_count; i++) {
        fw_rule_t *r = &fw_rules[i];
        int src_match  = (r->src   == 0 || r->src   == src);
        int dst_match  = (r->dst   == 0 || r->dst   == dst);
        int port_match = (r->dport == 0 || r->dport == dport);
        if (src_match && dst_match && port_match) return r->allow;
    }
    return 0; /* default deny */
}

/* ---- Tests ---- */

static void test_checksum_ipv4_header(void) {
    /* Minimal IPv4 header with TTL=64, proto=TCP, no options */
    uint8_t hdr[20] = {
        0x45, 0x00, 0x00, 0x3c,  /* ver/ihl, dscp/ecn, total length */
        0xab, 0xcd, 0x40, 0x00,  /* id, flags/frag */
        0x40, 0x06, 0x00, 0x00,  /* TTL=64, proto=TCP, checksum=0 */
        0xc0, 0xa8, 0x01, 0x01,  /* src 192.168.1.1 */
        0xc0, 0xa8, 0x01, 0x02,  /* dst 192.168.1.2 */
    };
    uint16_t csum = inet_checksum(hdr, 20);
    /* Fill checksum back in */
    hdr[10] = (uint8_t)(csum >> 8);
    hdr[11] = (uint8_t)(csum & 0xFF);
    /* Re-checksum: should be 0 (valid) */
    assert(inet_checksum(hdr, 20) == 0);
    printf("  [PASS] IPv4 header checksum round-trip\n");
}

static void test_checksum_empty(void) {
    assert(inet_checksum(NULL, 0) == 0xFFFF);
    printf("  [PASS] Checksum of empty payload is 0xFFFF\n");
}

static void test_arp_insert_lookup(void) {
    uint8_t mac[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    uint32_t ip = 0xC0A80101; /* 192.168.1.1 */
    arp_insert(ip, mac);
    const uint8_t *got = arp_lookup(ip);
    assert(got != NULL);
    assert(memcmp(got, mac, 6) == 0);
    printf("  [PASS] ARP cache insert and lookup\n");
}

static void test_arp_update(void) {
    uint8_t mac1[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    uint8_t mac2[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    uint32_t ip = 0xC0A80102;
    arp_insert(ip, mac1);
    arp_insert(ip, mac2); /* should update */
    const uint8_t *got = arp_lookup(ip);
    assert(got != NULL);
    assert(memcmp(got, mac2, 6) == 0);
    printf("  [PASS] ARP cache entry updated on duplicate IP\n");
}

static void test_arp_miss(void) {
    assert(arp_lookup(0xDEADBEEF) == NULL);
    printf("  [PASS] ARP cache miss returns NULL\n");
}

static void test_firewall_default_deny(void) {
    fw_rule_count = 0;
    assert(fw_check(0x01020304, 0x05060708, 80) == 0);
    printf("  [PASS] Default-deny firewall blocks traffic without rules\n");
}

static void test_firewall_allow_rule(void) {
    fw_rule_count = 0;
    fw_add_rule(0, 0, 80, 1);  /* allow all to port 80 */
    assert(fw_check(0x01020304, 0x05060708, 80) == 1);
    assert(fw_check(0x01020304, 0x05060708, 443) == 0);
    printf("  [PASS] Firewall allows port-80 traffic when rule present\n");
}

static void test_firewall_deny_explicit(void) {
    fw_rule_count = 0;
    fw_add_rule(0, 0, 80,  1);
    fw_add_rule(0, 0, 22,  0);  /* explicitly deny SSH */
    fw_add_rule(0, 0,  0,  1);  /* allow everything else */
    /* SSH should match deny rule first (rules evaluated in order) */
    assert(fw_check(0, 0, 22)  == 0);
    assert(fw_check(0, 0, 443) == 1);
    printf("  [PASS] First-match firewall rule ordering is respected\n");
}

int main(void) {
    printf("[test_net] Running networking unit tests...\n");
    test_checksum_ipv4_header();
    test_checksum_empty();
    test_arp_insert_lookup();
    test_arp_update();
    test_arp_miss();
    test_firewall_default_deny();
    test_firewall_allow_rule();
    test_firewall_deny_explicit();
    printf("[test_net] All tests passed\n");
    return 0;
}
