#!/usr/bin/env bash
# AegisOS v33 guard: refuse stale trees when testing syscall return-to-user path.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "error: v33 milestone guard: missing required file: $path" >&2
    exit 33
  fi
}

need_grep() {
  local pattern="$1"
  local path="$2"
  if ! grep -qE "$pattern" "$path"; then
    echo "error: v33 milestone guard: $path does not contain expected pattern: $pattern" >&2
    exit 33
  fi
}

need_file VERSION
need_file boot/arm64/el0_transition.S
need_file kernel/core/userland.c
need_file kernel/include/userland.h
need_file kernel/core/exception.c
need_file kernel/core/kernel_main.c
need_file tools/qemu/prove-boot-phases-aarch64.sh

need_grep 'v33|syscall-return-path' VERSION
need_grep 'aegis_first_user_process_after_syscall' boot/arm64/el0_transition.S
need_grep 'aegis_first_user_process_return_probe' boot/arm64/el0_transition.S
need_grep 'userland_mark_first_user_process_syscall_return' kernel/core/userland.c
need_grep 'userland_mark_first_user_process_continued_after_syscall' kernel/core/userland.c
need_grep 'userland_first_tiny_process_return_probe_active' kernel/core/exception.c
need_grep 'qemu_boot_checkpoint_syscall_return_path_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_syscall_return_path_ready' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'aegis_first_user_process_after_syscall' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_first_user_process_syscall_return' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_first_user_process_continued_after_syscall' tools/qemu/prove-boot-phases-aarch64.sh

echo "v33 milestone guard: OK ($(cat VERSION))"
