# AegisOS Router Runtime on an Arch Linux Development Host

This development path runs the real AegisOS service manager as the only stack
supervisor. A single host systemd unit launches that manager after boot; systemd
does not manage the individual AegisOS services.

## What this runtime creates

- `/run/service-manager.sock` — live service-manager control socket
- `/run/aegisd.sock` — privileged daemon API
- `aegis-lan0` — NetworkManager bridge with `192.168.88.1/24`
- `/run/aegisos-router/dnsmasq.conf` — generated DHCP/DNS configuration
- nftables tables `inet aegis` and, when an uplink exists, `ip aegis_nat`
- a host-safe development input policy that leaves the laptop reachable while enforcing router forwarding/NAT rules
- `wg0` — local WireGuard interface when `wireguard-tools` is available
- persistent RustMyAdmin data at `/var/lib/rustmyadmin/config.db`

The active laptop WAN connection is detected and preserved. The development
profile does not replace or recreate that host connection. Stopping the stack
removes only AegisOS-owned runtime resources: `aegis-lan`, the Aegis nftables
tables, `wg0`, generated dnsmasq files, and the two Unix sockets.

## Install and start

From the AegisOS repository root:

```sh
bash ./tools/dev/install-aegisos-host-stack.sh
```

The installer builds the native binaries, installs required Arch packages,
creates a local WireGuard configuration when one does not already exist,
creates a root-only credential file, and enables `aegisos-dev-stack.service`.

## Operate

```sh
./tools/dev/aegisos-host-stackctl.sh status
./tools/dev/aegisos-host-stackctl.sh logs
./tools/dev/aegisos-host-stackctl.sh services
./tools/dev/aegisos-host-stackctl.sh router
./tools/dev/aegisos-host-stackctl.sh restart
```

Dashboard: `http://127.0.0.1:4090/login`

RustMyAdmin: `http://127.0.0.1:8443/login`

Credentials are stored in `/etc/aegisos/dev.env` with mode `0600`.

## LTE/SIM boundary

Software can install and start NetworkManager and ModemManager, preserve the
Giffgaff APN profile, and expose truthful modem state. It cannot create a radio.
`radio.state = "hardware-missing"` remains correct until a compatible USB or
M.2 LTE modem with a SIM is physically attached and detected by ModemManager.
