/* SPDX-License-Identifier: Proprietary */
#pragma once
/* AegisOS live network bring-up and router control plane.
 *
 * The old v46 implementation fabricated a fixed lease.  PR1 requires the
 * state below to be sourced from the Ethernet driver and an actual DHCP
 * exchange.  A failed or absent link remains visible as not-ready and never
 * silently falls back to a synthetic address.
 */
#include "../../kernel/include/types.h"

#define AEGIS_NETCTL_IFACE_NAME_MAX 16U
#define AEGIS_NETCTL_PROFILE_MAX    32U

typedef enum aegis_netctl_link_state {
    AEGIS_NETCTL_LINK_DOWN = 0,
    AEGIS_NETCTL_LINK_CONFIGURING,
    AEGIS_NETCTL_LINK_UP,
    AEGIS_NETCTL_LINK_FAILED,
} aegis_netctl_link_state_t;

typedef struct aegis_network_control_state {
    bool initialised;
    bool hardware_io_ready;
    bool ethernet_identity_ready;
    bool dhcp_discover_prepared;
    bool dhcp_request_prepared;
    bool dhcp_bound;
    bool dhcp_timed_out;
    bool ipv4_configured;
    bool route_table_ready;
    bool nat_enabled;
    bool firewall_policy_loaded;
    bool control_plane_ready;
    bool user_kernel_isolation_preserved;
    char wan_if[AEGIS_NETCTL_IFACE_NAME_MAX];
    char lan_if[AEGIS_NETCTL_IFACE_NAME_MAX];
    char profile[AEGIS_NETCTL_PROFILE_MAX];
    aegis_netctl_link_state_t wan_link_state;
    u8 mac[6];
    u32 lease_ip;
    u32 subnet_mask;
    u32 gateway;
    u32 dns_server;
    u32 lease_seconds;
    u32 dhcp_attempts;
    u32 route_count;
    u32 firewall_rule_count;
    u32 nat_mapping_count;
    u32 control_plane_generation;
    u64 last_bringup_ns;
} aegis_network_control_state_t;

void network_control_init(void);
int  network_control_prepare_v46_bringup(void);
int  network_control_bringup_live(u64 timeout_ms);
void network_control_poll(void);
int  network_control_selftest(void);

const aegis_network_control_state_t *network_control_state(void);
bool network_control_ready(void);
bool network_control_dhcp_bound(void);
bool network_control_nat_ready(void);
bool network_control_firewall_ready(void);
const char *network_control_link_state_name(aegis_netctl_link_state_t state);
