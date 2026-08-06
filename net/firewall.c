/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/firewall.c
 * Stateful packet filter — rule-based traffic control with connection
 * tracking. Default-deny policy with ordered rule evaluation.
 */
#include "include/firewall.h"
#include "../kernel/include/memory.h"

#define FW_MAX_RULES       256
#define FW_MAX_CONN_TRACK  4096
#define FW_PROTO_TCP       6
#define FW_PROTO_UDP       17
#define FW_PROTO_ICMP      1

/* Connection tracking entry */
typedef struct conn_track {
    u32 src_ip;
    u32 dst_ip;
    u16 src_port;
    u16 dst_port;
    u8  protocol;
    u8  state;     /* 0=new, 1=established, 2=related */
    u32 last_seen; /* Tick count */
    bool active;
} conn_track_t;

/* IPv4 header (for extracting fields from packet) */
typedef struct {
    u8  version_ihl;
    u8  dscp_ecn;
    u16 total_len;
    u16 id;
    u16 flags_offset;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    u32 src;
    u32 dst;
} __attribute__((packed)) fw_ipv4_hdr_t;

static fw_rule_t *rule_head;
static fw_rule_t rule_storage[FW_MAX_RULES];
static u32 rule_count = 0;
static conn_track_t conn_table[FW_MAX_CONN_TRACK];
static fw_action_t default_policy = FW_DROP;
static bool fw_enabled = true;

int firewall_init(void) {
    rule_head = NULL;
    rule_count = 0;
    for (int i = 0; i < FW_MAX_CONN_TRACK; i++) {
        conn_table[i].active = false;
    }
    default_policy = FW_DROP;
    fw_enabled = true;
    return 0;
}

void firewall_set_default_policy(fw_action_t policy) {
    default_policy = policy;
}

void firewall_enable(bool enable) {
    fw_enabled = enable;
}

static bool ip_match(u32 addr, u32 rule_ip, u32 rule_mask) {
    if (rule_ip == 0 && rule_mask == 0) return true; /* Wildcard */
    return (addr & rule_mask) == (rule_ip & rule_mask);
}

static u16 extract_src_port(const u8 *transport_hdr) {
    return (u16)((transport_hdr[0] << 8) | transport_hdr[1]);
}

static u16 extract_dst_port(const u8 *transport_hdr) {
    return (u16)((transport_hdr[2] << 8) | transport_hdr[3]);
}

/* Connection tracking lookup */
static conn_track_t *ct_lookup(u32 src, u32 dst, u16 sport, u16 dport, u8 proto) {
    for (int i = 0; i < FW_MAX_CONN_TRACK; i++) {
        if (!conn_table[i].active) continue;
        /* Match forward direction */
        if (conn_table[i].src_ip == src && conn_table[i].dst_ip == dst &&
            conn_table[i].src_port == sport && conn_table[i].dst_port == dport &&
            conn_table[i].protocol == proto) {
            return &conn_table[i];
        }
        /* Match reverse direction (reply) */
        if (conn_table[i].src_ip == dst && conn_table[i].dst_ip == src &&
            conn_table[i].src_port == dport && conn_table[i].dst_port == sport &&
            conn_table[i].protocol == proto) {
            return &conn_table[i];
        }
    }
    return NULL;
}

static conn_track_t *ct_create(u32 src, u32 dst, u16 sport, u16 dport, u8 proto) {
    for (int i = 0; i < FW_MAX_CONN_TRACK; i++) {
        if (!conn_table[i].active) {
            conn_table[i].src_ip = src;
            conn_table[i].dst_ip = dst;
            conn_table[i].src_port = sport;
            conn_table[i].dst_port = dport;
            conn_table[i].protocol = proto;
            conn_table[i].state = 0; /* New */
            conn_table[i].active = true;
            return &conn_table[i];
        }
    }
    return NULL; /* Table full */
}

fw_action_t firewall_check(const pktbuf_t *pkt) {
    if (!fw_enabled) return FW_ACCEPT;
    if (!pkt || pkt->len < 20) return default_policy;

    /* Parse IP header */
    const fw_ipv4_hdr_t *ip = (const fw_ipv4_hdr_t *)(pkt->data + pkt->head);
    u8 ihl = (ip->version_ihl & 0xF) * 4;
    u32 src_ip = ip->src;
    u32 dst_ip = ip->dst;
    u8 protocol = ip->protocol;

    /* Extract ports for TCP/UDP */
    u16 src_port = 0, dst_port = 0;
    if ((protocol == FW_PROTO_TCP || protocol == FW_PROTO_UDP) && pkt->len >= ihl + 4) {
        const u8 *transport = pkt->data + pkt->head + ihl;
        src_port = extract_src_port(transport);
        dst_port = extract_dst_port(transport);
    }

    /* Check connection tracking — established connections pass */
    conn_track_t *ct = ct_lookup(src_ip, dst_ip, src_port, dst_port, protocol);
    if (ct && ct->state >= 1) {
        ct->last_seen = 0; /* Update timestamp */
        return FW_ACCEPT;
    }

    /* Evaluate rules in order */
    for (fw_rule_t *r = rule_head; r; r = r->next) {
        /* Match source IP */
        if (!ip_match(src_ip, r->src_ip, r->src_mask)) continue;
        /* Match destination IP */
        if (!ip_match(dst_ip, r->dst_ip, r->dst_mask)) continue;
        /* Match protocol (0 = any) */
        if (r->protocol != 0 && r->protocol != protocol) continue;
        /* Match ports (0 = any) */
        if (r->src_port != 0 && r->src_port != src_port) continue;
        if (r->dst_port != 0 && r->dst_port != dst_port) continue;

        /* Rule matched */
        if (r->action == FW_ACCEPT) {
            /* Create connection tracking entry */
            if (!ct) {
                ct = ct_create(src_ip, dst_ip, src_port, dst_port, protocol);
                if (ct) ct->state = 1;
            }
        }
        return r->action;
    }

    return default_policy;
}

int firewall_add_rule(fw_rule_t *rule) {
    if (!rule || rule_count >= FW_MAX_RULES) return -1;

    /* Copy rule to storage */
    rule_storage[rule_count] = *rule;
    fw_rule_t *stored = &rule_storage[rule_count];
    stored->next = NULL;
    rule_count++;

    /* Append to end of list (rules evaluated in order) */
    if (!rule_head) {
        rule_head = stored;
    } else {
        fw_rule_t *p = rule_head;
        while (p->next) p = p->next;
        p->next = stored;
    }
    return 0;
}

int firewall_del_rule(u32 rule_id) {
    if (rule_id >= rule_count) return -1;

    /* Remove from linked list */
    fw_rule_t *target = &rule_storage[rule_id];
    if (rule_head == target) {
        rule_head = target->next;
    } else {
        for (fw_rule_t *p = rule_head; p; p = p->next) {
            if (p->next == target) {
                p->next = target->next;
                break;
            }
        }
    }
    target->next = NULL;
    return 0;
}

int firewall_flush_rules(void) {
    rule_head = NULL;
    rule_count = 0;
    return 0;
}

void firewall_ct_cleanup(u32 max_age) {
    for (int i = 0; i < FW_MAX_CONN_TRACK; i++) {
        if (conn_table[i].active && conn_table[i].last_seen > max_age) {
            conn_table[i].active = false;
        }
    }
}

u32 firewall_rule_count(void) {
    return rule_count;
}
