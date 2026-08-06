#!/usr/bin/env bash
# AegisOS v31 line guard: refuse to run an old tree when testing controlled EL0.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "error: v31 tree guard: missing required file: $path" >&2
    exit 31
  fi
}

need_grep() {
  local pattern="$1"
  local path="$2"
  if ! grep -qE "$pattern" "$path"; then
    echo "error: v31 tree guard: $path does not contain expected pattern: $pattern" >&2
    exit 31
  fi
}

need_file VERSION
need_file boot/arm64/el0_transition.S
need_file kernel/core/userland.c
need_file kernel/include/userland.h
need_file tools/qemu/prove-boot-phases-aarch64.sh

need_grep 'v31\.[01]|controlled EL0 transition' VERSION
need_grep 'aegis_el0_transition_enter' boot/arm64/el0_transition.S
need_grep 'aegis_el0_transition_test_entry' boot/arm64/el0_transition.S
need_grep 'aegisbox_init_controlled_el0_transition_or_panic' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_el0_transition_prepared' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_el0_eret_launch' kernel/core/kernel_main.c
need_grep 'userland_mark_el0_svc_trap' kernel/core/userland.c
need_grep 'qemu_boot_checkpoint_fake_user_process_descriptor' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'qemu_boot_checkpoint_el0_transition_prepared' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_el0_svc_trap' tools/qemu/prove-boot-phases-aarch64.sh

echo "v31 tree guard: OK ($(cat VERSION))"
