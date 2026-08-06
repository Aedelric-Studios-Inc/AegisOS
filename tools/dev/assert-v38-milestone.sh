#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_grep() {
  local pat="$1" file="$2"
  if ! grep -q "$pat" "$file"; then
    echo "error: v38 milestone guard failed: missing '$pat' in $file" >&2
    exit 2
  fi
}
if [[ ! -f VERSION ]] || ! grep -Eq 'v38-.*argv.*multi' VERSION; then
  echo "error: v38 milestone guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_grep 'AEGIS_USER_ARGV_MAX' kernel/include/userland.h
need_grep 'AEGIS_USER_ENVP_MAX' kernel/include/userland.h
need_grep 'AEGIS_USER_PROCESS_ARGV_ENVP_READY' kernel/include/userland.h
need_grep 'AEGIS_USER_PROCESS_SECONDARY_DESCRIPTOR_READY' kernel/include/userland.h
need_grep 'user_stack_argv_envp_ready' kernel/include/userland.h
need_grep 'multiple_process_descriptors_ready' kernel/include/userland.h
need_grep 'userland_prepare_user_stack_bootstrap' kernel/core/userland.c
need_grep 'userland_user_stack_bootstrap_selftest' kernel/core/userland.c
need_grep 'userland_create_multiple_process_descriptors' kernel/core/userland.c
need_grep 'userland_multiple_process_descriptors_selftest' kernel/core/userland.c
need_grep 'qemu_boot_checkpoint_user_stack_argv_envp_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_multiple_user_process_descriptors_ready' kernel/core/kernel_main.c
need_grep 'userland_prepare_user_stack_bootstrap' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_create_multiple_process_descriptors' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'QEMU v38 proof accepted' tools/qemu/prove-boot-phases-aarch64.sh
echo "v38 milestone guard: OK (AegisOS-router-bringup-v38-argv-envp-multiprocess)"
