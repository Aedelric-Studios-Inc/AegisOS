#!/usr/bin/env bash
# AegisOS v34 guard: refuse stale trees when testing tiny user process exit.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "error: v34 milestone guard: missing required file: $path" >&2
    exit 34
  fi
}

need_grep() {
  local pattern="$1"
  local path="$2"
  if ! grep -qE "$pattern" "$path"; then
    echo "error: v34 milestone guard: $path does not contain expected pattern: $pattern" >&2
    exit 34
  fi
}

need_file VERSION
need_file boot/arm64/el0_transition.S
need_file kernel/core/userland.c
need_file kernel/include/userland.h
need_file kernel/core/exception.c
need_file kernel/core/kernel_main.c
need_file tools/qemu/prove-boot-phases-aarch64.sh

need_grep 'v34|tiny-user-process-exit' VERSION
need_grep 'aegis_first_user_process_exit_call' boot/arm64/el0_transition.S
need_grep 'mov x8, #4' boot/arm64/el0_transition.S
need_grep 'aegis_first_user_process_after_exit' boot/arm64/el0_transition.S
need_grep 'userland_mark_first_user_process_exit' kernel/core/userland.c
need_grep 'userland_first_tiny_process_exit_seen' kernel/core/userland.c
need_grep 'SYS_EXIT' kernel/core/exception.c
need_grep 'qemu_boot_checkpoint_first_user_process_exit_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_first_user_process_exit_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'aegis_first_user_process_exit_call' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_first_user_process_exit' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'after_exit' tools/qemu/prove-boot-phases-aarch64.sh

echo "v34 milestone guard: OK ($(cat VERSION))"
