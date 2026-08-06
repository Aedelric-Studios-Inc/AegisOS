#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_grep() {
  local pat="$1" file="$2"
  if ! grep -q "$pat" "$file"; then
    echo "error: v37 milestone guard failed: missing '$pat' in $file" >&2
    exit 2
  fi
}
if [[ ! -f VERSION ]] || ! grep -Eq 'v37(\.1)?-.*ptload' VERSION; then
  echo "error: v37 milestone guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_grep 'text_pt_load_copied' kernel/include/elf_loader.h
need_grep 'elf_loader_pt_load_selftest' kernel/core/elf_loader.c
need_grep 'copy_executable_pt_load' kernel/core/elf_loader.c
need_grep 'first_user_text_backing' kernel/core/elf_loader.c
need_grep 'userland_bind_first_process_to_loaded_elf' kernel/core/userland.c
need_grep 'userland_pt_load_permissions_selftest' kernel/core/userland.c
need_grep 'AEGIS_USER_REGION_MAPPED' kernel/include/userland.h
need_grep 'elf_pt_load_segments_copied' kernel/core/kernel_main.c
need_grep 'user_kernel_page_permissions_ready' kernel/core/kernel_main.c
need_grep 'elf_pt_load_segments_copied' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'userland_bind_first_process_to_loaded_elf' tools/qemu/prove-boot-phases-aarch64.sh
need_grep 'user_kernel_page_permissions_ready' tools/qemu/prove-boot-phases-aarch64.sh
echo "v37.1 milestone guard: OK (AegisOS-router-bringup-v37.1-ptload-proofscript-fix)"
