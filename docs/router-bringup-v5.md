# AegisOS Router Bring-up v5

This pass removes the router scaffolding and replaces it with command-backed
AegisBox/ÆÐELRIC Router integration points. It is still a bring-up build, not a
consumer firmware release, but the router stack now has real backends.

## Implemented router roles

`system/config/network.toml` defines the canonical router role map:

- `wan = "eth0"`
- `lan = "eth1"`
- `wifi = "wlan0"`
- `wwan = "wwan0"`
- `bluetooth = "bt0"`

The daemon exposes this through:

- `GET /network/interfaces`
- `POST /network/apply`
- `GET /router/status`
- `POST /router/provision`

## Real Linux router backends

AegisOS v5 uses established Linux tooling for the AegisBox production target:

- NetworkManager via `nmcli` for WAN/LAN/LTE connection profiles
- ModemManager via `mmcli` for LTE/SIM modem discovery and status
- nftables via `nft` for firewall/NAT
- dnsmasq for LAN DNS/DHCP
- WireGuard tools via `wg` and `wg-quick`

This keeps AegisBox realistic now, while preserving the abstractions PhoenixOS
can reuse later.

## Safe apply mode

By default, router apply/provision paths use dry-run mode unless:

```sh
export AEGISOS_ROUTER_DRY_RUN=0
```

This avoids accidentally rewriting networking/firewall state on a development
machine.

## CLI

```sh
aegisctl router status
aegisctl router provision
aegisctl network interfaces
aegisctl network apply
aegisctl radio status
aegisctl radio connect
aegisctl radio disconnect
aegisctl dns status
aegisctl dns reload
aegisctl firewall apply
aegisctl vpn status
aegisctl vpn up
aegisctl vpn down
```

## RustMyAdmin

RustMyAdmin now has a `/router` page and `/api/router` endpoint that query
`aegisd` for router, interface, radio, DNS/DHCP, VPN and firewall status.

## Remaining hardware validation

Before calling this production firmware, validate on actual AegisBox hardware:

1. Interface names match the hardware target or udev mappings.
2. `mmcli -L` sees the LTE carrier module.
3. `nmcli connection up aegis-lte-giffgaff` attaches on the Giffgaff APN.
4. nftables baseline allows LAN admin access and NATs to WAN/WWAN.
5. dnsmasq leases LAN clients and serves configured upstream DNS.
6. WireGuard status/up/down work through real `wg-quick` config.
7. Recovery path still works after bad network/firewall configuration.
