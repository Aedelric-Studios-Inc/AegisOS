#!/usr/bin/env sh
set -eu

# AegisOS Router first-boot provisioning.
# Safe by default: set AEGISOS_ROUTER_DRY_RUN=0 to apply changes on hardware.
: "${AEGISOS_ROUTER_DRY_RUN:=1}"
export AEGISOS_ROUTER_DRY_RUN

log() { printf '[aegis-router-provision] %s\n' "$*"; }
need() { command -v "$1" >/dev/null 2>&1 || log "missing tool: $1"; }

need nmcli
need mmcli
need nft
need dnsmasq
need wg
need wg-quick

if command -v aegisctl >/dev/null 2>&1; then
  log "provisioning through aegisd/aegisctl"
  aegisctl router provision || true
  aegisctl router status || true
else
  log "aegisctl unavailable; applying minimal sysctl fallback"
  if [ "$AEGISOS_ROUTER_DRY_RUN" = "0" ]; then
    printf '1\n' > /proc/sys/net/ipv4/ip_forward || true
  fi
fi

log "done"
