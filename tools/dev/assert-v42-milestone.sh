#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_file() { [[ -f "$1" ]] || { echo "error: v42 guard failed: missing $1" >&2; exit 2; }; }
need_grep() { grep -q "$1" "$2" || { echo "error: v42 guard failed: missing '$1' in $2" >&2; exit 2; }; }
if [[ ! -f VERSION ]] || ! grep -Eq 'AegisOS-v2\.0-v42-multiprocess-syscall-expansion' VERSION; then
  echo "error: v42 guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_file kernel/include/block_storage.h
need_file kernel/core/block_storage.c
need_file docs/BLOCK_STORAGE_ABSTRACTION.md
need_file docs/PROCESS_TABLE_CLEANUP.md
need_file docs/V42_RELEASE_NOTES.md
need_grep 'userland_prepare_real_multiprocess_launch_path' kernel/core/kernel_main.c
need_grep 'userland_prepare_syscall_expansion' kernel/core/kernel_main.c
need_grep 'SYS_SPAWN' kernel/include/syscalls.h
need_grep 'SYS_WAITPID' kernel/include/syscalls.h
need_grep 'SYS_SERVICE_ID' kernel/include/syscalls.h
need_grep 'qemu_boot_checkpoint_real_multiprocess_launch_path_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'qemu_boot_checkpoint_syscall_expansion_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'QEMU v42 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'assert-v42-milestone.sh' tools/qemu/build-and-smoke-aarch64.sh
if compgen -G 'README-v*.md' >/dev/null; then
  echo "error: v42 guard failed: root milestone README-v*.md files should remain consolidated" >&2
  exit 2
fi
echo "v42 milestone guard: OK (AegisOS-v2.0-v42-multiprocess-syscall-expansion)"
