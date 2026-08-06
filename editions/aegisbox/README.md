# AegisBox Edition

AegisBox is the first production target for AegisOS.

It is intentionally constrained: networking, firewall, DNS, VPN, LTE/SIM failover,
updates, recovery, auditing, and RustMyAdmin control. It is the safe proving ground
for AegisOS core behaviour before PhoenixOS inherits the reusable kernel/core ideas.

## Production priorities

1. Bootable AegisBox image.
2. `aegisd` as the local system-control daemon.
3. RustMyAdmin as the authenticated admin surface.
4. Ethernet/Wi-Fi/LTE routing and failover.
5. Firewall, NAT, DNS filtering, VPN, monitoring.
6. Signed updates and recovery boot path.
7. Audit logs and least-privilege service policy.
