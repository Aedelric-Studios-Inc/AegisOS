#!/usr/bin/env bash
# AegisOS v32 guard: refuse stale trees when testing first tiny user process.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "error: v32 milestone guard: missing required file: $path" >&2
    exit 32
  fi
}

need_grep() {
  local pattern="$1"
  local path="$2"
  if ! grep -qE "$pattern" "$path"; then
    echo "error: v32 milestone guard: $path does not contain expected pattern: $pattern" >&2
    exit 32
  fi
}

need_file VERSION
need_file boot/arm64/el0_transition.S
need_file kernel/core/userland.c
need_file kernel/include/userland.h
need_file kernel/core/exception.c
need_file tools/qemu/prove-boot-phases-aarch64.sh

need_grep 'v32|first-tiny-user-process' VERSION
need_grep 'aegis_first_user_process_entry' boot/arm64/el0_transition.S
need_grep 'userland_prepare_first_tiny_user_process' kernel/core/userland.c
need_grep 'userland_mark_first_user_process_syscall' kernel/core/userland.c
need_grep 'userland_first_tiny_process_active' kernel/core/exception.c
need_grep 'aegisbox_init_first_tiny_user_process_or_panic' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_first_user_process_descriptor' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_first_user_process_launch' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'aegis_first_user_process_entry' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_first_user_process_syscall' tools/qemu/prove-boot-phases-aarch64.sh

echo "v32 milestone guard: OK ($(cat VERSION))"
