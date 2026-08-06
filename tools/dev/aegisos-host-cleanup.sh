#!/usr/bin/env sh
set -u

VPN_INTERFACE=${AEGIS_VPN_INTERFACE:-wg0}

if command -v wg-quick >/dev/null 2>&1; then
    wg-quick down "$VPN_INTERFACE" >/dev/null 2>&1 || true
fi
if command -v nft >/dev/null 2>&1; then
    nft delete table inet aegis >/dev/null 2>&1 || true
    nft delete table ip aegis_nat >/dev/null 2>&1 || true
fi
if command -v nmcli >/dev/null 2>&1; then
    nmcli connection down aegis-lan >/dev/null 2>&1 || true
    nmcli connection delete aegis-lan >/dev/null 2>&1 || true
fi

rm -f /run/aegisd.sock \
      /run/service-manager.sock \
      /run/aegisos-router/dnsmasq.pid \
      /run/aegisos-router/dnsmasq.conf
