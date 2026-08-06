# AegisOS v46 — Network Bring-up + DHCP + NAT/Firewall Control Plane

AegisOS v46 adds the first QEMU-proven router/network control-plane contract on top of v45 user/kernel page-table isolation.

## Proved scope

- Ethernet stack identity prepared for the router profile.
- ARP/UDP/TCP/routing/firewall/NAT scaffolding initialised in a deterministic order.
- Synthetic DHCP lease proof records the DISCOVER/OFFER/REQUEST/ACK outcome without depending on live external network I/O.
- IPv4 address, netmask, gateway, and DNS state are committed into the network control state.
- Default route is registered.
- Firewall control plane loads a default-deny policy with explicit DHCP, DNS, and HTTPS egress allowance.
- NAT is enabled and a deterministic port-forward proof entry is installed.
- v45 isolation and the v44 supervisor boundary are preserved before EL0 launch/exit proof.

## Expected QEMU proof line

```text
QEMU v46 proof accepted: network stack brought up, synthetic DHCP lease bound, NAT/firewall control plane loaded, and v45 isolation chain preserved.
```

## Boundary

v46 is the router control-plane proof. It does not require live QEMU packet traffic, a real DHCP server, or physical NIC I/O. Real DHCP packet exchange and board-specific NIC validation remain hardware/network-backend work after the Developer Preview image line.
