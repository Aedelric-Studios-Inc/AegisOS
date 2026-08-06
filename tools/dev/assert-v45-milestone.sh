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

need_file kernel/include/page_isolation.h
need_file kernel/memory/page_isolation.c
need_grep 'page_isolation_prepare_user_kernel_tables' kernel/memory/page_isolation.c
need_grep 'page_table_entry_allows_user' kernel/memory/page_tables.c
need_grep 'qemu_boot_checkpoint_v45_page_tables_allocated' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v45_user_regions_mapped' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v45_kernel_window_blocked' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v45_guard_pages_unmapped' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_v45_ttbr0_contract_ready' kernel/core/kernel_main.c
need_grep 'QEMU v45 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'AegisOS-v2.0-v45-user-kernel-page-table-isolation' VERSION

echo "v45 milestone guard: OK (AegisOS-v2.0-v45-user-kernel-page-table-isolation)"
