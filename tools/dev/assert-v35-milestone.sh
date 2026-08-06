#!/usr/bin/env bash
# AegisOS v35 guard: refuse stale trees when testing user-process section completion + ELF loader.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

need_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "error: v35 milestone guard: missing required file: $path" >&2
    exit 35
  fi
}

need_grep() {
  local pattern="$1"
  local path="$2"
  if ! grep -qE "$pattern" "$path"; then
    echo "error: v35 milestone guard: $path does not contain expected pattern: $pattern" >&2
    exit 35
  fi
}

need_file VERSION
need_file kernel/include/elf_loader.h
need_file kernel/core/elf_loader.c
need_file kernel/include/userland.h
need_file kernel/core/userland.c
need_file kernel/core/exception.c
need_file kernel/core/kernel_main.c
need_file tools/qemu/prove-boot-phases-aarch64.sh

need_grep 'v35|elf-loader|section-complete' VERSION
need_grep 'elf_loader_load_builtin_aegis_init' kernel/core/elf_loader.c
need_grep 'AEGIS_ELF_EM_AARCH64' kernel/include/elf_loader.h
need_grep 'userland_bind_first_process_to_elf' kernel/core/userland.c
need_grep 'userland_first_process_elf_loader_selftest' kernel/core/userland.c
need_grep 'userland_mark_post_exit_scheduler_handoff' kernel/core/exception.c
need_grep 'qemu_boot_checkpoint_elf_loader_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_elf_aegis_init_loaded' kernel/core/kernel_main.c
need_grep 'elf_loader_load_builtin_aegis_init' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_mark_post_exit_scheduler_handoff' tools/qemu/prove-boot-phases-aarch64.sh

echo "v35 milestone guard: OK ($(cat VERSION))"
