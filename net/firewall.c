/* SPDX-License-Identifier: Proprietary */
/* AegisOS — net/firewall.c */
#include "include/firewall.h"

static fw_rule_t *rule_head;

int firewall_init(void) { rule_head = NULL; return 0; }

fw_action_t firewall_check(const pktbuf_t *pkt) {
    (void)pkt;
    /* Default deny — evaluate rules in order */
    for (fw_rule_t *r = rule_head; r; r = r->next) {
        /* TODO: match fields */
        return r->action;
    }
    return FW_ACCEPT;
}

int firewall_add_rule(fw_rule_t *rule) {
    rule->next = rule_head;
    rule_head  = rule;
    return 0;
}

int firewall_del_rule(u32 rule_id) { (void)rule_id; return 0; }
