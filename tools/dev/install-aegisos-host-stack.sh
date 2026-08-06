#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "$0")/../.." && pwd)
UNIT=/etc/systemd/system/aegisos-dev-stack.service
ENV_FILE=/etc/aegisos/dev.env

for command in cargo openssl systemctl; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Missing required command: $command" >&2
        exit 1
    fi
done

cd "$ROOT"

chmod 0755     "$ROOT/tools/dev/aegisos-host-stack.sh"     "$ROOT/tools/dev/aegisos-host-cleanup.sh"     "$ROOT/tools/dev/aegisos-host-stackctl.sh"     "$ROOT/tools/dev/bootstrap-router-host.sh"     "$ROOT/tools/dev/router-provision.sh"

bash "$ROOT/tools/dev/bootstrap-router-host.sh"

echo "Building native AegisOS development services..."
cargo build --release --manifest-path userland/Cargo.toml
cargo build --release --manifest-path system/daemon/aegisd/Cargo.toml
cargo build --release --manifest-path system/cli/aegisctl/Cargo.toml
cargo build --release --manifest-path dashboard/Cargo.toml
cargo build --release --manifest-path rustmyadmin/Cargo.toml
for manifest in services/*/Cargo.toml; do
    cargo build --release --manifest-path "$manifest"
done

sudo install -d -m 0750 /etc/aegisos
sudo install -d -m 0755 /run/aegisos-router /var/www/aegisos
sudo install -d -m 0750 /var/lib/rustmyadmin

local_db="${HOME}/.local/state/rustmyadmin/config.db"
if [[ -f "$local_db" ]] && ! sudo test -f /var/lib/rustmyadmin/config.db; then
    sudo install -m 0600 "$local_db" /var/lib/rustmyadmin/config.db
    echo "Migrated the existing RustMyAdmin database to /var/lib/rustmyadmin/config.db."
fi

dashboard_token=$(sudo sed -n 's/^AEGIS_DASHBOARD_TOKEN="\(.*\)"$/\1/p' "$ENV_FILE" 2>/dev/null | head -n 1 || true)
admin_password=$(sudo sed -n 's/^RUSTMYADMIN_ADMIN_PASSWORD="\(.*\)"$/\1/p' "$ENV_FILE" 2>/dev/null | head -n 1 || true)
if [[ -z "$dashboard_token" ]]; then
    dashboard_token=$(openssl rand -hex 32)
fi
if [[ -z "$admin_password" ]]; then
    admin_password=$(openssl rand -hex 24)
fi

tmp=$(mktemp)
cat > "$tmp" <<EOF_ENV
AEGIS_SERVICES_CONFIG="$ROOT/etc/services.dev.toml"
AEGIS_SERVICE_MANAGER_SOCKET="/run/service-manager.sock"
AEGISD_SOCKET="/run/aegisd.sock"
AEGISD_DEV_TRUST_LOCAL="1"
AEGISD_SOCKET_MODE="0660"
AEGISOS_NETWORK_CONFIG="$ROOT/etc/network.dev.toml"
AEGISOS_DNSMASQ_CONFIG="/run/aegisos-router/dnsmasq.conf"
AEGISOS_DNSMASQ_PID="/run/aegisos-router/dnsmasq.pid"
AEGIS_DNSMASQ_UPSTREAM="127.0.0.1#53530"
AEGISOS_ROUTER_DRY_RUN="0"
AEGISOS_HOST_SAFE_FIREWALL="1"
AEGIS_VPN_INTERFACE="wg0"
AEGIS_DASHBOARD_TOKEN="$dashboard_token"
RUSTMYADMIN_ADMIN_PASSWORD="$admin_password"
RUSTMYADMIN_DB_PATH="/var/lib/rustmyadmin/config.db"
EOF_ENV
sudo install -m 0600 "$tmp" "$ENV_FILE"
rm -f "$tmp"

unit_tmp=$(mktemp)
cat > "$unit_tmp" <<EOF_UNIT
[Unit]
Description=AegisOS development service stack
Documentation=file://$ROOT/docs/router-runtime-host.md
After=network-online.target NetworkManager.service ModemManager.service
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=$ROOT
EnvironmentFile=$ENV_FILE
ExecStart=$ROOT/tools/dev/aegisos-host-stack.sh
Restart=on-failure
RestartSec=2
KillMode=mixed
TimeoutStopSec=20
ExecStopPost=$ROOT/tools/dev/aegisos-host-cleanup.sh

[Install]
WantedBy=multi-user.target
EOF_UNIT
sudo install -m 0644 "$unit_tmp" "$UNIT"
rm -f "$unit_tmp"

sudo systemctl stop aegisos-dev-stack.service 2>/dev/null || true
if sudo pgrep -x service-manager >/dev/null 2>&1; then
    echo "Stopping the manually launched AegisOS service manager before systemd takes ownership..."
    sudo pkill -TERM -x service-manager
    for _ in {1..40}; do
        sudo pgrep -x service-manager >/dev/null 2>&1 || break
        sleep 0.1
    done
fi
sudo pkill -TERM -x aegisd 2>/dev/null || true
sudo rm -f /run/aegisd.sock /run/service-manager.sock

sudo systemctl daemon-reload
sudo systemctl enable --now aegisos-dev-stack.service

echo
echo "AegisOS host stack installed and started."
echo "Dashboard:   http://127.0.0.1:4090/login"
echo "RustMyAdmin: http://127.0.0.1:8443/login"
echo "Credentials are stored root-only in $ENV_FILE:"
sudo grep -E '^(AEGIS_DASHBOARD_TOKEN|RUSTMYADMIN_ADMIN_PASSWORD)=' "$ENV_FILE"
