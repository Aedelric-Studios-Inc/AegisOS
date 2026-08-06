#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
need_grep() {
  local pat="$1" file="$2"
  if ! grep -q "$pat" "$file"; then
    echo "error: v36 milestone guard failed: missing '$pat' in $file" >&2
    exit 2
  fi
}
if [[ ! -f VERSION ]] || ! grep -Eq 'v36(\.1)?-file-backed-initramfs-elf|v36\.1-file-backed-initramfs-elf-alignfix' VERSION; then
  echo "error: v36 milestone guard failed: stale or wrong VERSION" >&2
  exit 2
fi
need_grep 'initramfs_install_builtin_userland' fs/initramfs.c
need_grep 'elf_loader_load_vfs_aegis_init' kernel/core/elf_loader.c
need_grep 'qemu_boot_checkpoint_initramfs_file_table_ready' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_vfs_open_aegis_init' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_vfs_read_aegis_init' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_elf_loaded_from_initramfs' kernel/core/kernel_main.c
need_grep 'qemu_boot_checkpoint_first_user_process_from_file' kernel/core/kernel_main.c
echo "v36.1 milestone guard: OK (AegisOS-router-bringup-v36.1-file-backed-initramfs-elf-alignfix)"
