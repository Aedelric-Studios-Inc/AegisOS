#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    exec sudo -- "$0" "$@"
fi

if ! command -v pacman >/dev/null 2>&1; then
    echo "This bootstrap currently supports Arch Linux hosts." >&2
    exit 1
fi

pacman -S --needed --noconfirm \
    nftables \
    dnsmasq \
    wireguard-tools \
    networkmanager \
    modemmanager \
    iproute2 \
    openssl

systemctl enable --now NetworkManager.service
systemctl enable --now ModemManager.service

install -d -m 0755 /run/aegisos-router
install -d -m 0750 /var/lib/rustmyadmin
install -d -m 0755 /var/www/aegisos
install -d -m 0700 /etc/wireguard

if [[ ! -f /etc/wireguard/wg0.conf ]]; then
    umask 077
    private_key=$(wg genkey)
    cat > /etc/wireguard/wg0.conf <<EOF_WG
[Interface]
PrivateKey = ${private_key}
Address = 10.77.0.1/24
ListenPort = 51820
SaveConfig = false
EOF_WG
    chmod 0600 /etc/wireguard/wg0.conf
    echo "Created a local-only WireGuard interface config at /etc/wireguard/wg0.conf."
else
    echo "Preserving existing /etc/wireguard/wg0.conf."
fi

if [[ ! -f /var/www/aegisos/index.html ]]; then
    cat > /var/www/aegisos/index.html <<'EOF_HTML'
<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>AegisOS Web Hosting</title></head>
<body><h1>AegisOS web-hosting service is running.</h1></body></html>
EOF_HTML
fi

echo "Host router dependencies are installed."
echo "LTE remains hardware-dependent: ModemManager is ready, but a compatible modem/SIM must be physically attached."
