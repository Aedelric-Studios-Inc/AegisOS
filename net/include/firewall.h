/* SPDX-License-Identifier: Proprietary */
#pragma once
/* net/include/firewall.h — Stateful packet filter interface */

#include "../../kernel/include/types.h"
#include "packet.h"

typedef enum { FW_ACCEPT = 0, FW_DROP = 1, FW_REJECT = 2 } fw_action_t;

typedef struct fw_rule {
    u32  src_ip;
    u32  src_mask;
    u32  dst_ip;
    u32  dst_mask;
    u16  src_port;
    u16  dst_port;
    u8   protocol;
    fw_action_t action;
    struct fw_rule *next;
} fw_rule_t;

int        firewall_init(void);
fw_action_t firewall_check(const pktbuf_t *pkt);
int        firewall_add_rule(fw_rule_t *rule);
int        firewall_del_rule(u32 rule_id);
