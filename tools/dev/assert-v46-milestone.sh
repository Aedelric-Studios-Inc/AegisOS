#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  [[ -f "$1" ]] || { echo "missing: $1" >&2; exit 2; }
}
need_grep() {
  local pattern="$1" file="$2"
  grep -Fq "$pattern" "$file" || { echo "missing pattern in $file: $pattern" >&2; exit 2; }
}

need_file net/include/network_control.h
need_file net/network_control.c
need_grep 'network_control_prepare_v46_bringup' net/network_control.c
need_grep 'network_control_dhcp_bound' net/network_control.c
need_grep 'network_control_nat_ready' net/network_control.c
need_grep 'network_control_firewall_ready' net/network_control.c
need_grep 'qemu_boot_checkpoint_v46_network_stack_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v46_dhcp_bound' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v46_nat_firewall_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v46_router_control_plane_ready' kernel/core/kernel_main.c
need_grep 'QEMU v46 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh

echo "v46 milestone guard: OK (network bring-up + DHCP + NAT/firewall control plane)"
