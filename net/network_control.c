/* SPDX-License-Identifier: Proprietary */
/* AegisOS live Ethernet/DHCP/routing/NAT/firewall bring-up. */
#include "include/network_control.h"
#include "include/dhcp.h"
#include "include/firewall.h"
#include "include/routing.h"
#include "include/socket.h"
#include "../hal/include/ethernet.h"
#include "../kernel/include/rtc.h"

extern int  net_ethernet_init(void);
extern void ethernet_get_local_mac(u8 *mac);
extern void arp_init(void);
extern void udp_init(void);
extern int  udp_bind(u16 port);
extern int  udp_recv(int sock_id, u8 *buf, u16 max_len, u32 *src_ip, u16 *src_port);
extern void tcp_init(void);
extern void ipv4_set_addr(u32 ip);
extern void ipv4_set_netmask(u32 mask);
extern void ipv4_set_gateway(u32 gw);
extern u32  ipv4_get_addr(void);
extern int  routing_set_default(u32 gateway, u32 iface_ip);
extern u32  routing_count(void);
extern void firewall_set_default_policy(fw_action_t policy);
extern void firewall_enable(bool enable);
extern u32  firewall_rule_count(void);
extern void nat_init(void);
extern void nat_enable(u32 wan_ip);
extern u32  nat_active_count(void);

#define DHCP_CLIENT_PORT 68U
#define DHCP_RETRY_NS    1000000000ULL
#define DEFAULT_TIMEOUT_MS 5000ULL
#define AEGIS_PROTO_TCP 6U
#define AEGIS_PROTO_UDP 17U

static aegis_network_control_state_t g;
static int dhcp_socket = -1;

static void zero_bytes(void *ptr,u64 len){u8*p=ptr;for(u64 i=0;i<len;i++)p[i]=0;}
static void copy_cstr(char*d,u64 n,const char*s){u64 i=0;if(!d||!n)return;if(s)for(;i+1<n&&s[i];i++)d[i]=s[i];d[i]=0;}

const char *network_control_link_state_name(aegis_netctl_link_state_t s){
    switch(s){case AEGIS_NETCTL_LINK_DOWN:return "down";case AEGIS_NETCTL_LINK_CONFIGURING:return "configuring";case AEGIS_NETCTL_LINK_UP:return "up";case AEGIS_NETCTL_LINK_FAILED:return "failed";default:return "unknown";}
}

void network_control_init(void){
    zero_bytes(&g,sizeof(g));
    copy_cstr(g.wan_if,sizeof(g.wan_if),"eth0");
    copy_cstr(g.lan_if,sizeof(g.lan_if),"eth1");
    copy_cstr(g.profile,sizeof(g.profile),"aegisbox-live");
    g.wan_link_state=AEGIS_NETCTL_LINK_DOWN;
    g.initialised=true;
    dhcp_socket=-1;
}

static int install_firewall_policy(void){
    fw_rule_t r;
    zero_bytes(&r,sizeof(r));r.protocol=AEGIS_PROTO_UDP;r.src_port=67U;r.dst_port=68U;r.action=FW_ACCEPT;if(firewall_add_rule(&r)!=0)return AEGIS_EIO;
    zero_bytes(&r,sizeof(r));r.protocol=AEGIS_PROTO_UDP;r.dst_port=53U;r.action=FW_ACCEPT;if(firewall_add_rule(&r)!=0)return AEGIS_EIO;
    zero_bytes(&r,sizeof(r));r.protocol=AEGIS_PROTO_TCP;r.dst_port=443U;r.action=FW_ACCEPT;if(firewall_add_rule(&r)!=0)return AEGIS_EIO;
    zero_bytes(&r,sizeof(r));r.protocol=AEGIS_PROTO_TCP;r.dst_port=8443U;r.action=FW_ACCEPT;if(firewall_add_rule(&r)!=0)return AEGIS_EIO;
    return AEGIS_OK;
}

void network_control_poll(void){
    if(!g.initialised||!g.hardware_io_ready)return;
    network_poll();
    if(dhcp_socket>=0){
        u8 buf[768];u32 src=0;u16 port=0;
        for(;;){int n=udp_recv(dhcp_socket,buf,sizeof(buf),&src,&port);if(n<0)break;dhcp_handle_response(buf,(u16)n);}
    }
    if(dhcp_get_state()==DHCP_STATE_REQUESTING)g.dhcp_request_prepared=true;
    if(dhcp_get_state()==DHCP_STATE_BOUND&&!g.dhcp_bound){
        g.lease_ip=dhcp_get_assigned_ip();g.subnet_mask=dhcp_get_subnet_mask();g.gateway=dhcp_get_gateway();g.dns_server=dhcp_get_dns_server();g.lease_seconds=dhcp_get_lease_seconds();g.dhcp_bound=(g.lease_ip!=0U);
    }
}

static int finish_control_plane(void){
    if(!g.dhcp_bound)return AEGIS_EAGAIN;
    ipv4_set_addr(g.lease_ip);ipv4_set_netmask(g.subnet_mask);ipv4_set_gateway(g.gateway);
    g.ipv4_configured=(ipv4_get_addr()==g.lease_ip);
    if(!g.ipv4_configured||g.gateway==0U)return AEGIS_EIO;
    if(routing_set_default(g.gateway,g.lease_ip)!=0)return AEGIS_EIO;
    g.route_count=routing_count();g.route_table_ready=(g.route_count>0U);
    firewall_set_default_policy(FW_DROP);if(install_firewall_policy()!=AEGIS_OK)return AEGIS_EIO;firewall_enable(true);
    g.firewall_rule_count=firewall_rule_count();g.firewall_policy_loaded=(g.firewall_rule_count>=4U);
    nat_enable(g.lease_ip);g.nat_mapping_count=nat_active_count();g.nat_enabled=true;
    g.user_kernel_isolation_preserved=true;g.control_plane_generation++;
    g.control_plane_ready=g.hardware_io_ready&&g.dhcp_bound&&g.ipv4_configured&&g.route_table_ready&&g.firewall_policy_loaded&&g.nat_enabled;
    g.wan_link_state=g.control_plane_ready?AEGIS_NETCTL_LINK_UP:AEGIS_NETCTL_LINK_FAILED;
    return g.control_plane_ready?AEGIS_OK:AEGIS_EIO;
}

int network_control_bringup_live(u64 timeout_ms){
    if(!g.initialised)return AEGIS_EINVAL;
    if(!ethernet_link_ready()){g.wan_link_state=AEGIS_NETCTL_LINK_DOWN;return AEGIS_ENOENT;}
    g.hardware_io_ready=true;
    if(net_ethernet_init()!=0)return AEGIS_EIO;
    ethernet_get_local_mac(g.mac);g.ethernet_identity_ready=true;g.wan_link_state=AEGIS_NETCTL_LINK_CONFIGURING;
    arp_init();udp_init();tcp_init();if(routing_init()!=0||firewall_init()!=0)return AEGIS_EIO;firewall_enable(false);nat_init();
    dhcp_socket=udp_bind(DHCP_CLIENT_PORT);if(dhcp_socket<0)return AEGIS_EBUSY;
    if(timeout_ms==0U)timeout_ms=DEFAULT_TIMEOUT_MS;
    u64 start=monotonic_nanoseconds(),deadline=start+timeout_ms*1000000ULL,next_retry=start;
    do{
        u64 now=monotonic_nanoseconds();
        if(now>=next_retry&&dhcp_get_state()!=DHCP_STATE_BOUND){
            if(dhcp_discover()==0){g.dhcp_discover_prepared=true;g.dhcp_attempts++;}
            next_retry=now+DHCP_RETRY_NS;
        }
        network_control_poll();
        if(g.dhcp_bound){g.last_bringup_ns=monotonic_nanoseconds();return finish_control_plane();}
        __asm__ volatile("yield");
    }while(monotonic_nanoseconds()<deadline);
    g.dhcp_timed_out=true;g.wan_link_state=AEGIS_NETCTL_LINK_FAILED;g.last_bringup_ns=monotonic_nanoseconds();
    return AEGIS_ETIMEDOUT;
}

int network_control_prepare_v46_bringup(void){return network_control_bringup_live(DEFAULT_TIMEOUT_MS);}
int network_control_selftest(void){
    if(!g.initialised||!g.hardware_io_ready||!g.ethernet_identity_ready)return AEGIS_EINVAL;
    if(!g.dhcp_discover_prepared||!g.dhcp_request_prepared||!g.dhcp_bound)return AEGIS_EINVAL;
    if(!g.ipv4_configured||ipv4_get_addr()!=g.lease_ip)return AEGIS_EINVAL;
    if(!g.route_table_ready||!g.firewall_policy_loaded||!g.nat_enabled||!g.control_plane_ready)return AEGIS_EINVAL;
    return g.wan_link_state==AEGIS_NETCTL_LINK_UP?AEGIS_OK:AEGIS_EINVAL;
}
const aegis_network_control_state_t*network_control_state(void){return &g;}
bool network_control_ready(void){return g.control_plane_ready;}
bool network_control_dhcp_bound(void){return g.dhcp_bound;}
bool network_control_nat_ready(void){return g.nat_enabled;}
bool network_control_firewall_ready(void){return g.firewall_policy_loaded;}
