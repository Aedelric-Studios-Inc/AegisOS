#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_file() { [[ -f "$1" ]] || { echo "error: v41 guard failed: missing $1" >&2; exit 2; }; }
need_grep() { grep -q "$1" "$2" || { echo "error: v41 guard failed: missing '$1' in $2" >&2; exit 2; }; }
if [[ ! -f VERSION ]] || ! grep -Eq 'AegisOS-v2\.0-v41-block-storage-process-table' VERSION; then
  echo "error: v41 guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_file kernel/include/block_storage.h
need_file kernel/core/block_storage.c
need_file docs/BLOCK_STORAGE_ABSTRACTION.md
need_file docs/PROCESS_TABLE_CLEANUP.md
need_file docs/V41_RELEASE_NOTES.md
need_file tools/image/build-aegisos-v2-flash-img.sh
need_file tools/qemu/prove-v40-disk-image-aarch64.sh
need_grep 'block_storage_register_v40_flash_layout' kernel/core/kernel_main.c
need_grep 'process_table_cleanup_prepare' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_block_storage_abstraction_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'qemu_boot_checkpoint_process_table_cleanup_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'QEMU v41 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'assert-v41-milestone.sh' tools/qemu/build-and-smoke-aarch64.sh
if compgen -G 'README-v*.md' >/dev/null; then
  echo "error: v41 guard failed: root milestone README-v*.md files should remain consolidated" >&2
  exit 2
fi
echo "v41 milestone guard: OK (AegisOS-v2.0-v41-block-storage-process-table)"
