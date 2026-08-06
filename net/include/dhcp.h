/* SPDX-License-Identifier: Proprietary */
#pragma once
#include "../../kernel/include/types.h"

typedef enum {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
} dhcp_state_t;

int dhcp_discover(void);
int dhcp_request(u32 offered_ip);
void dhcp_handle_response(const u8 *data, u16 len);
int dhcp_release(void);
dhcp_state_t dhcp_get_state(void);
u32 dhcp_get_assigned_ip(void);
u32 dhcp_get_subnet_mask(void);
u32 dhcp_get_gateway(void);
u32 dhcp_get_dns_server(void);
u32 dhcp_get_lease_seconds(void);
