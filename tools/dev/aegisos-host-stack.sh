#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

: "${AEGIS_SERVICES_CONFIG:=$ROOT/etc/services.dev.toml}"
: "${AEGIS_SERVICE_MANAGER_SOCKET:=/run/service-manager.sock}"
: "${AEGISD_SOCKET:=/run/aegisd.sock}"
: "${AEGISOS_NETWORK_CONFIG:=$ROOT/etc/network.dev.toml}"
: "${AEGISOS_DNSMASQ_CONFIG:=/run/aegisos-router/dnsmasq.conf}"
: "${AEGISOS_DNSMASQ_PID:=/run/aegisos-router/dnsmasq.pid}"
: "${AEGIS_DNSMASQ_UPSTREAM:=127.0.0.1#53530}"
: "${AEGISOS_ROUTER_DRY_RUN:=0}"
: "${AEGISOS_HOST_SAFE_FIREWALL:=1}"
: "${AEGIS_VPN_INTERFACE:=wg0}"
: "${RUSTMYADMIN_DB_PATH:=/var/lib/rustmyadmin/config.db}"

export AEGIS_SERVICES_CONFIG
export AEGIS_SERVICE_MANAGER_SOCKET
export AEGISD_SOCKET
export AEGISOS_NETWORK_CONFIG
export AEGISOS_DNSMASQ_CONFIG
export AEGISOS_DNSMASQ_PID
export AEGIS_DNSMASQ_UPSTREAM
export AEGISOS_ROUTER_DRY_RUN
export AEGISOS_HOST_SAFE_FIREWALL
export AEGIS_VPN_INTERFACE
export RUSTMYADMIN_DB_PATH

mkdir -p /run/aegisos-router /var/lib/rustmyadmin /var/www/aegisos
rm -f "$AEGIS_SERVICE_MANAGER_SOCKET" "$AEGISD_SOCKET"

cd "$ROOT"
exec "$ROOT/userland/target/release/service-manager"
